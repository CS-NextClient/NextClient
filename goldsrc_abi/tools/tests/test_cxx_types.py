import unittest

from goldsrc_tools import cxx_types


class ParseDeclareTest(unittest.TestCase):
    def test_round_trip_with_a_name(self):
        cases = {
            "const char *": "const char* name",
            "char *const": "char* const name",
            "float[3]": "float name[3]",
            "char[32][16]": "char name[32][16]",
            "model_t *[512]": "model_t* name[512]",
            "int (*)": "int* name",
            "void (*)()": "void (*name)()",
            "void (*)(int, int)": "void (*name)(int, int)",
            "void (*)(const char *, ...)": "void (*name)(const char*, ...)",
            "unsigned char *(*)(const char *, int, int *)": "unsigned char* (*name)(const char*, int, int*)",
            "float ****(*)()": "float**** (*name)()",
            "void (*)(const char *, void (*)(struct event_args_s *))":
                "void (*name)(const char*, void (*)(struct event_args_s*))",
            "struct model_s *(*)(const char *, int *)": "struct model_s* (*name)(const char*, int*)",
            "int (const char *, int, void *)": "int name(const char*, int, void*)",
            "wrect_t &(int)": "wrect_t& name(int)",
            "const char * const *": "const char* const* name",
            "int *const *": "int* const* name",
            "void (* const *)(int)": "void (*const* name)(int)",
        }
        for text, expected in cases.items():
            self.assertEqual(cxx_types.declare(cxx_types.parse(text), "name"), expected, text)

    def test_member_function_qualifiers_and_conventions(self):
        method = cxx_types.parse("int (float) __attribute__((thiscall)) const")
        self.assertEqual(cxx_types.declare(method, "Draw"), "int Draw(float) const")
        self.assertEqual(method.convention, "thiscall")
        # clang-format (-style=file, repository .clang-format) reflows the brief's own expectation
        # of "void (__stdcall* hook)(int)" by dropping the space before '(': verified below.
        stdcall = cxx_types.parse("void (*)(int) __attribute__((stdcall))")
        self.assertEqual(cxx_types.declare(stdcall, "hook"), "void(__stdcall* hook)(int)")

    def test_base_override_prints_an_inline_record_body(self):
        type_ = cxx_types.parse("struct (unnamed at common/studio.h:232:2)[4]")
        self.assertTrue(cxx_types.is_unnamed(type_.inner.text))
        self.assertEqual(cxx_types.unnamed_key(type_.inner.text), "common/studio.h:232:2")
        self.assertEqual(cxx_types.declare(type_, "num", base="struct\n{\n    int x;\n}"),
                         "struct\n{\n    int x;\n} num[4]")

    def test_empty_name_array_declarator_glues_to_the_bracket(self):
        """A `using` alias never carries a name, so its array dimension is the very first thing in the
        declarator; real clang-format rejects the space this used to leave (found on `vec3_t`)."""
        self.assertEqual(cxx_types.declare(cxx_types.parse("float[3]"), ""), "float[3]")
        self.assertEqual(cxx_types.declare(cxx_types.parse("char[32][16]"), ""), "char[32][16]")


class RenameTest(unittest.TestCase):
    def test_renaming_reaches_nested_signatures(self):
        mapping = {"cl_entity_s": "cl_entity_t", "byte": "uint8_t"}.get
        type_ = cxx_types.parse("void (*)(struct cl_entity_s *, byte)")
        self.assertEqual(cxx_types.declare(cxx_types.rename(type_, mapping), "fn"),
                         "void (*fn)(cl_entity_t*, uint8_t)")

    def test_unmapped_names_and_builtins_are_kept(self):
        type_ = cxx_types.parse("const char *")
        self.assertEqual(cxx_types.declare(cxx_types.rename(type_, {}.get), "text"), "const char* text")


class ReferenceTest(unittest.TestCase):
    def test_pointers_references_and_signatures_do_not_need_a_definition(self):
        cases = {
            "struct cvar_s *": [("cvar_s", True)],
            "entity_state_t": [("entity_state_t", False)],
            "physent_t[32]": [("physent_t", False)],
            "void (*)(struct cl_entity_s *, usercmd_t)": [("cl_entity_s", True), ("usercmd_t", True)],
            "const struct rect_s *": [("rect_s", True)],
            "std::vector<int>": [("std::vector", False)],
        }
        for text, expected in cases.items():
            self.assertEqual(cxx_types.references(cxx_types.parse(text)), expected, text)


class TemplateArgumentTest(unittest.TestCase):
    """A template argument list's own commas must not be mistaken for parameter separators."""

    def test_multi_argument_template_parameter_is_not_split_on_its_inner_comma(self):
        type_ = cxx_types.parse("void (*)(int, std::pair<int, float>, char)")
        parameters = [parameter.text for parameter in type_.inner.parameters]
        self.assertEqual(parameters, ["int", "std::pair<int, float>", "char"])
        self.assertEqual(cxx_types.declare(type_, "name"), "void (*name)(int, std::pair<int, float>, char)")

    def test_nested_template_argument(self):
        type_ = cxx_types.parse("void (*)(std::vector<std::pair<int, float>>, int)")
        parameters = [parameter.text for parameter in type_.inner.parameters]
        self.assertEqual(parameters, ["std::vector<std::pair<int, float>>", "int"])

    def test_function_pointer_inside_a_template_argument(self):
        type_ = cxx_types.parse("void (*)(std::function<void (*)(int)>, int)")
        parameters = [parameter.text for parameter in type_.inner.parameters]
        self.assertEqual(parameters, ["std::function<void (*)(int)>", "int"])

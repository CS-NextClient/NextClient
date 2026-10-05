use std::{env, fs, path::PathBuf};

fn main() {
    let schema = "../../tests/abi_layout.def";
    println!("cargo:rerun-if-changed={schema}");
    if env::var_os("CARGO_FEATURE_ABI_TESTS").is_none() {
        return;
    }
    let mut source =
        String::from("fn abi_layout() -> String { let mut values = Vec::<String>::new();\n");
    for line in fs::read_to_string(schema).expect("ABI schema").lines() {
        let Some((kind, args)) = line.split_once('(') else {
            continue;
        };
        let parts: Vec<_> = args
            .trim_end_matches(')')
            .split(',')
            .map(str::trim)
            .collect();
        let (c, r) = (parts[0], parts[1]);
        if kind == "NC_ABI_TYPE" {
            for (name, operation) in [("sizeof", "size_of"), ("alignof", "align_of")] {
                source += &format!(
                    "values.push(format!(\"\\\"{c}.{name}\\\":{{}}\", std::mem::{operation}::<nc::{r}>()));\n"
                );
            }
        } else if kind == "NC_ABI_FIELD" {
            let field = parts[2];
            source += &format!(
                "values.push(format!(\"\\\"{c}.{field}\\\":{{}}\", std::mem::offset_of!(nc::{r}, {field})));\n"
            );
        } else {
            panic!("Unknown ABI schema entry: {kind}");
        }
    }
    source += "format!(\"{{{}}}\", values.join(\",\")) }\n";
    fs::write(
        PathBuf::from(env::var_os("OUT_DIR").unwrap()).join("abi_layout.rs"),
        source,
    )
    .unwrap();
}

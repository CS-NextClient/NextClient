using System.Text.Json;

namespace TestMasterServer;

// A request body of the form {"method": "server_list", "data": {"extended": true}}; clients that predate the
// extended layout send "data": "null"
internal readonly record struct MasterRequest(string? Method, bool Extended)
{
    public const string ServerListMethod = "server_list";

    // Method is null for a body that is not a JSON object with a string method
    public static MasterRequest Parse(byte[] body)
    {
        try
        {
            using JsonDocument document = JsonDocument.Parse(body);
            JsonElement root = document.RootElement;

            if (root.ValueKind != JsonValueKind.Object)
            {
                return default;
            }

            string? method = root.TryGetProperty("method", out JsonElement methodElement) && methodElement.ValueKind == JsonValueKind.String
                ? methodElement.GetString()
                : null;

            bool extended = root.TryGetProperty("data", out JsonElement data) &&
                            data.ValueKind == JsonValueKind.Object &&
                            data.TryGetProperty("extended", out JsonElement flag) &&
                            flag.ValueKind == JsonValueKind.True;

            return new MasterRequest(method, extended);
        }
        catch (JsonException)
        {
            return default;
        }
    }
}

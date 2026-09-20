using System.Diagnostics.CodeAnalysis;
using System.Net;
using System.Net.Sockets;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace TestMasterServer;

internal enum ServerMode
{
    // An extended request gets the JSON layout, any other request the text layout
    Extended,

    // Every request gets the text layout, as from a backend that does not know the extended flag
    Legacy,
}

// The servers file holds the list in the extended layout: [{"address": "ip:port", "country": ..., "game_mode": ...}]
internal sealed class MasterServer(IPAddress listenAddress, int port, string serversPath)
{
    private static readonly TimeSpan ConnectionTimeout = TimeSpan.FromSeconds(10);

    private readonly TcpListener _listener = new(listenAddress, port);
    private int _mode = (int)ServerMode.Extended;

    public ServerMode Mode
    {
        get => (ServerMode)Volatile.Read(ref _mode);
        set => Volatile.Write(ref _mode, (int)value);
    }

    public void Start()
    {
        _listener.Start();
    }

    public async Task RunAsync(CancellationToken cancellationToken)
    {
        try
        {
            while (true)
            {
                TcpClient client = await _listener.AcceptTcpClientAsync(cancellationToken);
                _ = HandleClientAsync(client, cancellationToken);
            }
        }
        catch (OperationCanceledException)
        {
        }
        finally
        {
            _listener.Stop();
        }
    }

    // The servers file parsed as a JSON array; error describes why it is not one
    public bool TryLoadServers([NotNullWhen(true)] out JsonArray? servers, [NotNullWhen(false)] out string? error)
    {
        servers = null;

        try
        {
            if (JsonNode.Parse(File.ReadAllText(serversPath)) is JsonArray array)
            {
                servers = array;
                error = null;
                return true;
            }

            error = "the file is not a JSON array";
        }
        catch (Exception e) when (e is IOException or UnauthorizedAccessException or JsonException)
        {
            error = e.Message;
        }

        return false;
    }

    public static string GetModeName(ServerMode mode)
    {
        return mode == ServerMode.Extended ? "extended" : "legacy";
    }

    private async Task HandleClientAsync(TcpClient client, CancellationToken cancellationToken)
    {
        using TcpClient connection = client;
        using CancellationTokenSource timeout = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        timeout.CancelAfter(ConnectionTimeout);

        string remote = connection.Client.RemoteEndPoint?.ToString() ?? "unknown";

        try
        {
            NetworkStream stream = connection.GetStream();

            HttpRequest? request = await HttpExchange.ReadRequestAsync(stream, timeout.Token);
            if (request is null)
            {
                return;
            }

            ServerMode mode = Mode;
            MasterRequest masterRequest = MasterRequest.Parse(request.Body);
            (HttpResponse response, string outcome) = Answer(masterRequest, mode);

            string buildVersion = request.Headers.GetValueOrDefault("BuildVersion", "-");
            string requestLayout = masterRequest.Extended ? "extended" : "legacy";

            Console.WriteLine(
                $"{GetTimestamp()} {remote} {request.Method} {request.Target} build={buildVersion} " +
                $"request={requestLayout} mode={GetModeName(mode)} -> {outcome}");

            await HttpExchange.WriteResponseAsync(stream, response, timeout.Token);
        }
        catch (Exception e) when (e is IOException or SocketException or InvalidDataException or OperationCanceledException)
        {
            Console.WriteLine($"{GetTimestamp()} {remote} request failed: {e.Message}");
        }
    }

    private (HttpResponse Response, string Outcome) Answer(MasterRequest request, ServerMode mode)
    {
        if (request.Method != MasterRequest.ServerListMethod)
        {
            return (HttpResponse.Text(400, "expected a server_list request\n"), "400, not a server_list request");
        }

        if (!TryLoadServers(out JsonArray? servers, out string? error))
        {
            return (HttpResponse.Text(500, error + "\n"), $"500, servers file: {error}");
        }

        if (request.Extended && mode == ServerMode.Extended)
        {
            return (HttpResponse.Json(200, servers.ToJsonString()), $"200, extended layout, {servers.Count} servers");
        }

        List<string> addresses = GetAddresses(servers);

        return (HttpResponse.Text(200, string.Concat(addresses.Select(address => address + "\n"))), $"200, text layout, {addresses.Count} servers");
    }

    // the string "address" of every entry that has one
    private static List<string> GetAddresses(JsonArray servers)
    {
        List<string> addresses = [];

        foreach (JsonNode? server in servers)
        {
            if (server is JsonObject entry && entry["address"] is JsonValue address && address.TryGetValue(out string? text))
            {
                addresses.Add(text);
            }
        }

        return addresses;
    }

    private static string GetTimestamp()
    {
        return DateTime.Now.ToString("HH:mm:ss.fff");
    }
}

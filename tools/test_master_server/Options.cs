using System.Net;

namespace TestMasterServer;

internal sealed record Options(IPAddress ListenAddress, int Port, string ServersPath)
{
    private const int DefaultPort = 27080;
    private const string DefaultServersPath = "servers.json";

    public const string Usage = "usage: TestMasterServer [--listen <address>] [--port <port>] [--servers <file>]";

    // null when an argument is unknown or its value does not parse
    public static Options? Parse(string[] args)
    {
        if (args.Length % 2 != 0)
        {
            return null;
        }

        IPAddress listenAddress = IPAddress.Loopback;
        int port = DefaultPort;
        string serversPath = DefaultServersPath;

        for (int i = 0; i < args.Length; i += 2)
        {
            string value = args[i + 1];

            switch (args[i])
            {
                case "--listen" when IPAddress.TryParse(value, out IPAddress? address):
                    listenAddress = address;
                    break;

                case "--port" when int.TryParse(value, out int number) && number is > 0 and <= ushort.MaxValue:
                    port = number;
                    break;

                case "--servers":
                    serversPath = value;
                    break;

                default:
                    return null;
            }
        }

        return new Options(listenAddress, port, Path.GetFullPath(serversPath));
    }
}

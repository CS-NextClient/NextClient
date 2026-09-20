using System.Net.Sockets;

namespace TestMasterServer;

internal static class Program
{
    private static async Task<int> Main(string[] args)
    {
        Options? options = Options.Parse(args);
        if (options is null)
        {
            Console.Error.WriteLine(Options.Usage);
            return 1;
        }

        MasterServer server = new(options.ListenAddress, options.Port, options.ServersPath);

        if (!server.TryLoadServers(out _, out string? error))
        {
            Console.Error.WriteLine($"{options.ServersPath}: {error}");
            return 1;
        }

        try
        {
            server.Start();
        }
        catch (SocketException e)
        {
            Console.Error.WriteLine($"cannot listen on {options.ListenAddress}:{options.Port}: {e.Message}");
            return 1;
        }

        using CancellationTokenSource shutdown = new();
        Task serverTask = server.RunAsync(shutdown.Token);

        string url = $"http://{options.ListenAddress}:{options.Port}/";
        Console.WriteLine($"Test master server listening on {url}");
        Console.WriteLine($"Servers are read from {options.ServersPath} on every request");
        Console.WriteLine($"Client setup: a Servers entry with \"address\" \"{url}\" in platform/config/MasterServer.vdf");
        PrintMode(server.Mode);
        PrintHelp();

        while (Console.ReadLine() is { } line)
        {
            string command = line.Trim().ToLowerInvariant();

            if (command is "quit" or "exit")
            {
                break;
            }

            ExecuteCommand(server, command);
        }

        shutdown.Cancel();
        await serverTask;

        return 0;
    }

    private static void ExecuteCommand(MasterServer server, string command)
    {
        switch (command)
        {
            case "":
                break;

            case "extended":
                server.Mode = ServerMode.Extended;
                PrintMode(server.Mode);
                break;

            case "legacy":
                server.Mode = ServerMode.Legacy;
                PrintMode(server.Mode);
                break;

            case "mode":
                PrintMode(server.Mode);
                break;

            case "help":
                PrintHelp();
                break;

            default:
                Console.WriteLine($"Unknown command \"{command}\"");
                PrintHelp();
                break;
        }
    }

    private static void PrintMode(ServerMode mode)
    {
        string description = mode == ServerMode.Extended
            ? "extended requests get the JSON layout, other requests the text layout"
            : "every request gets the text layout, as from a backend that does not know the extended flag";

        Console.WriteLine($"Mode: {MasterServer.GetModeName(mode)} ({description})");
    }

    private static void PrintHelp()
    {
        Console.WriteLine("Commands: extended | legacy | mode | help | quit");
    }
}

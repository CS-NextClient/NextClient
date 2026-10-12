using System.Net.Sockets;
using System.Text;

namespace TestMasterServer;

internal sealed record HttpRequest(string Method, string Target, IReadOnlyDictionary<string, string> Headers, byte[] Body);

internal sealed record HttpResponse(int Status, string ContentType, byte[] Body)
{
    public static HttpResponse Text(int status, string text)
    {
        return new HttpResponse(status, "text/plain; charset=utf-8", Encoding.UTF8.GetBytes(text));
    }

    public static HttpResponse Json(int status, string json)
    {
        return new HttpResponse(status, "application/json", Encoding.UTF8.GetBytes(json));
    }
}

// The part of HTTP/1.1 the master server clients use: one request per connection, a body sized by Content-Length
internal static class HttpExchange
{
    private const int MaxHeadBytes = 64 * 1024;
    private const int MaxBodyBytes = 1024 * 1024;

    private static readonly byte[] HeadTerminator = "\r\n\r\n"u8.ToArray();

    // null when the connection closes before a request starts
    public static async Task<HttpRequest?> ReadRequestAsync(NetworkStream stream, CancellationToken cancellationToken)
    {
        byte[] buffer = new byte[MaxHeadBytes];
        int length = 0;
        int headEnd;

        while ((headEnd = buffer.AsSpan(0, length).IndexOf(HeadTerminator)) < 0)
        {
            if (length == buffer.Length)
            {
                throw new InvalidDataException("the request head is too large");
            }

            int read = await stream.ReadAsync(buffer.AsMemory(length), cancellationToken);
            if (read == 0)
            {
                if (length == 0)
                {
                    return null;
                }

                throw new InvalidDataException("the connection closed inside the request head");
            }

            length += read;
        }

        string[] lines = Encoding.Latin1.GetString(buffer, 0, headEnd).Split("\r\n");
        string[] requestLine = lines[0].Split(' ');
        if (requestLine.Length != 3)
        {
            throw new InvalidDataException($"malformed request line \"{lines[0]}\"");
        }

        Dictionary<string, string> headers = new(StringComparer.OrdinalIgnoreCase);

        foreach (string line in lines.Skip(1))
        {
            int colon = line.IndexOf(':');
            if (colon > 0)
            {
                headers[line[..colon].Trim()] = line[(colon + 1)..].Trim();
            }
        }

        int bodyStart = headEnd + HeadTerminator.Length;
        byte[] body = await ReadBodyAsync(stream, headers, buffer.AsMemory(bodyStart, length - bodyStart), cancellationToken);

        return new HttpRequest(requestLine[0], requestLine[1], headers, body);
    }

    public static async Task WriteResponseAsync(NetworkStream stream, HttpResponse response, CancellationToken cancellationToken)
    {
        string head = $"HTTP/1.1 {response.Status} {GetReasonPhrase(response.Status)}\r\n" +
                      $"Content-Type: {response.ContentType}\r\n" +
                      $"Content-Length: {response.Body.Length}\r\n" +
                      "Connection: close\r\n\r\n";

        await stream.WriteAsync(Encoding.ASCII.GetBytes(head), cancellationToken);
        await stream.WriteAsync(response.Body, cancellationToken);
    }

    // received holds the body bytes that arrived together with the head
    private static async Task<byte[]> ReadBodyAsync(
        NetworkStream stream,
        Dictionary<string, string> headers,
        ReadOnlyMemory<byte> received,
        CancellationToken cancellationToken)
    {
        if (headers.ContainsKey("Transfer-Encoding"))
        {
            throw new InvalidDataException("chunked request bodies are not supported");
        }

        if (!headers.TryGetValue("Content-Length", out string? lengthText))
        {
            return [];
        }

        if (!int.TryParse(lengthText, out int length) || length < 0 || length > MaxBodyBytes)
        {
            throw new InvalidDataException($"unsupported Content-Length {lengthText}");
        }

        byte[] body = new byte[length];
        int copied = Math.Min(received.Length, length);
        received[..copied].CopyTo(body);

        if (copied < length && headers.TryGetValue("Expect", out string? expect) && expect.Equals("100-continue", StringComparison.OrdinalIgnoreCase))
        {
            await stream.WriteAsync("HTTP/1.1 100 Continue\r\n\r\n"u8.ToArray(), cancellationToken);
        }

        await stream.ReadExactlyAsync(body.AsMemory(copied), cancellationToken);

        return body;
    }

    private static string GetReasonPhrase(int status)
    {
        return status switch
        {
            200 => "OK",
            400 => "Bad Request",
            500 => "Internal Server Error",
            _ => "Unknown",
        };
    }
}

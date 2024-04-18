import base64
import hashlib
import hmac
import sys
import gzip
from StringIO import StringIO


def hmac_hash_with_secret(key, message):
    return base64.b64encode(hmac.new(key, message, digestmod=hashlib.sha256).digest())


def get_gzip_string(string_for_gzip):
    zip_text_file = StringIO()
    zipper = gzip.GzipFile(mode='wb', fileobj=zip_text_file)
    zipper.write(string_for_gzip)
    zipper.close()

    enc_text = zip_text_file.getvalue()
    return enc_text


def run():
    print hmac_hash_with_secret("test1", "[{\"v\":2,\"user_id\":\"test\"},{\"v\":3,\"user_id\":\"test2\"}]")
    compressed = get_gzip_string("[{\"v\":2,\"user_id\":\"test\"},{\"v\":3,\"user_id\":\"test2\"}]")
    s = ""
    for c in compressed:
        s += str(ord(c)) + ","
    print s

    sys.exit()


run()

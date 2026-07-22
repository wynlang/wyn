# AWS from Wyn - SigV4-signed REST, no SDK

Pure-Wyn AWS access: `sigv4.wyn` signs requests (AWS Signature Version 4),
`transport.wyn` sends them over HTTPS via curl config files (secrets never
touch a command line).

## Run

```sh
eval "$(aws configure export-credentials --profile <profile> --format env)"
wyn run demos/aws/sts_whoami.wyn        # who am I
wyn run demos/aws/s3_list_buckets.wyn   # aws s3 ls
wyn run demos/aws/ec2_regions.wyn       # all enabled EC2 regions
```

All demos are read-only.

## How the signing works

SigV4 derives a signing key by chaining four HMAC-SHA256 rounds:

    kDate    = HMAC("AWS4" + secret, date)
    kRegion  = HMAC(kDate, region)
    kService = HMAC(kRegion, service)
    kSigning = HMAC(kService, "aws4_request")

The intermediate digests are BINARY. Wyn strings are C strings, so binary
bytes cannot pass through string returns - instead each round returns hex and
the next round takes its key as hex:

    var kdate = Crypto.hmac_sha256("AWS4" + secret, datestamp)   // hex out
    var kregion = Crypto.hmac_sha256_hex(kdate, region)          // hex key in

Both builtins are native in-process HMAC-SHA256 (RFC 2104) over a native
SHA-256 (FIPS 180-4), validated against RFC 4231 test vectors in
`tests/expect/test_crypto_hmac_sha256.wyn`.

## Transport

`transport.request(cfg)` writes the signed request (URL and headers,
including the session token) into a temp file created with 0600 permissions,
then runs `curl --config <file>`. Only the file path appears in argv, so
credentials are never visible in the process list. The temp file is removed
after the call.

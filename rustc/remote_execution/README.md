To enable Remote Execution, add the following 4 entries to `.buckconfig.local`
in the repo root, using values given by your Remote Execution provider. The
format for some specific services is shown below, but anything that supports
Bazel's [remote execution API] is likely to work.

[remote execution API]: https://github.com/bazelbuild/remote-apis

## BuildBuddy

```ini
[buck2_re_client]
engine_address = remote.buildbuddy.io
action_cache_address = remote.buildbuddy.io
cas_address = remote.buildbuddy.io
http_headers = x-buildbuddy-api-key:zzzzzzzzzzzzzzzzzzzz
```

## NativeLink

```ini
[buck2_re_client]
engine_address = scheduler-whoami-zzzzzz.build-faster.nativelink.net
action_cache_address = cas-whoami-zzzzzz.build-faster.nativelink.net
cas_address = cas-whoami-zzzzzz.build-faster.nativelink.net
http_headers = x-nativelink-api-key:zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz
```

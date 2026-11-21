Reference documentation for the JSON-RPC API exposed by [Kodi](https://kodi.tv), the open source media center. It covers every request/response method, every server-initiated notification, every schema type and the error taxonomy, and is generated directly from the machine-readable schema shipped inside Kodi itself.

{how-it-works}

## Worked examples

Each example shows the exact envelopes on the wire. The curl command targets the HTTP transport; the same request envelope works over WebSocket and raw TCP verbatim.

{examples}

## Discovering the API at runtime

This site and the artifacts below describe a release. [JSONRPC.Introspect]({v}/methods/JSONRPC.Introspect.html) describes the instance you are connected to. Call it for three things.

{runtime-enums}

**What your connection may call.** Introspect reports the methods your permissions and your transport allow, not every method that exists. Calling one your permissions leave out returns [BadPermission]({v}/errors.html); one not served over the transport you used, or one that does not exist at all, returns MethodNotFound.

**Which version you are talking to.** Call `JSONRPC.Version`, then Introspect if you need the shape as well as the number. Do this before assuming any behaviour described here.

For code generation, offline tooling and comparing one release against another, use the artifacts below instead.

Introspect answers with the whole description by default, which is large. Narrow it:

```
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "JSONRPC.Introspect",
  "params": {
    "filter": {
      "type": "type",
      "id": "GUI.Window"
    },
    "getdescriptions": false
  }
}
```

`filter.type` accepts `method`, `namespace`, `type`, `notification` and `error`. `getdescriptions` and `getmetadata` strip the documentation out of the answer; a method's `deprecated` note is reported either way.

## About this documentation

This site is generated from the machine-readable schema shipped inside Kodi (`xbmc/interfaces/json-rpc/schema`) on every change, so it cannot drift from the implementation. The same schema is served live by a running Kodi instance via the [JSONRPC.Introspect]({v}/methods/JSONRPC.Introspect.html) method.

Machine-readable artifacts:

- [openrpc.json]({v}/openrpc.json) - [OpenRPC](https://open-rpc.org/) document covering all request/response methods
- [asyncapi.json]({v}/asyncapi.json) - [AsyncAPI](https://www.asyncapi.com/) document covering the notifications

## Upgrading

{upgrading-banner}

{upgrading-links}

## Reference

{reference}

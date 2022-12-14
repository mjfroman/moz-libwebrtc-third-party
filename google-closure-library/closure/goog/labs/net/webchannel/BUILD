load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "webchannel",
    srcs = [],
    exports = [
        ":webchannelbase",
        ":webchannelbasetransport",
    ],
)

closure_js_library(
    name = "channel",
    srcs = [
        "channel.js",
        "channelrequest.js",
    ],
    deps = [
        ":connectionstate",
        ":environment",
        ":requeststats",
        ":webchanneldebug",
        ":wire",
        "//closure/goog/asserts",
        "//closure/goog/async:throttle",
        "//closure/goog/disposable",
        "//closure/goog/events:event",
        "//closure/goog/events:eventhandler",
        "//closure/goog/labs/net:webchannel",
        "//closure/goog/net:errorcode",
        "//closure/goog/net:eventtype",
        "//closure/goog/net:xhrio",
        "//closure/goog/net:xmlhttp",
        "//closure/goog/object",
        "//closure/goog/string",
        "//closure/goog/timer",
        "//closure/goog/uri",
        "//closure/goog/useragent",
    ],
)

alias(
    name = "channelrequest",
    actual = ":channel",
)

closure_js_library(
    name = "connectionstate",
    srcs = ["connectionstate.js"],
)

closure_js_library(
    name = "environment",
    srcs = ["environment.js"],
    deps = ["//closure/goog/useragent"],
)

closure_js_library(
    name = "forwardchannelrequestpool",
    srcs = ["forwardchannelrequestpool.js"],
    deps = [
        ":channelrequest",
        ":wire",
        "//closure/goog/array",
        "//closure/goog/string",
    ],
)

closure_js_library(
    name = "netutils",
    srcs = ["netutils.js"],
    deps = [
        ":webchanneldebug",
        "//closure/goog/uri",
    ],
)

closure_js_library(
    name = "requeststats",
    srcs = ["requeststats.js"],
    deps = [
        "//closure/goog/events:event",
        "//closure/goog/events:eventtarget",
    ],
)

closure_js_library(
    name = "webchannelbase",
    srcs = ["webchannelbase.js"],
    deps = [
        ":channel",
        ":channelrequest",
        ":connectionstate",
        ":environment",
        ":forwardchannelrequestpool",
        ":netutils",
        ":requeststats",
        ":webchanneldebug",
        ":wire",
        ":wirev8",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/async:run",
        "//closure/goog/collections:maps",
        "//closure/goog/json",
        "//closure/goog/labs/net:webchannel",
        "//closure/goog/net:fetchxmlhttpfactory",
        "//closure/goog/net:xhrio",
        "//closure/goog/net:xmlhttpfactory",
        "//closure/goog/net/rpc:httpcors",
        "//closure/goog/object",
        "//closure/goog/string",
        "//closure/goog/structs",
        "//closure/goog/uri",
    ],
)

closure_js_library(
    name = "webchannelbasetransport",
    srcs = ["webchannelbasetransport.js"],
    deps = [
        ":channelrequest",
        ":webchannelbase",
        ":wire",
        "//closure/goog/asserts",
        "//closure/goog/events:eventtarget",
        "//closure/goog/json",
        "//closure/goog/labs/net:webchannel",
        "//closure/goog/labs/net:webchanneltransport",
        "//closure/goog/log",
        "//closure/goog/object",
        "//closure/goog/string",
        "//closure/goog/string:path",
    ],
)

closure_js_library(
    name = "webchanneldebug",
    srcs = ["webchanneldebug.js"],
    deps = [
        "//closure/goog/json",
        "//closure/goog/log",
        "//closure/goog/net:xmlhttp",
        "//closure/goog/uri",
    ],
)

closure_js_library(
    name = "wire",
    srcs = ["wire.js"],
    deps = ["//closure/goog/collections:maps"],
)

closure_js_library(
    name = "wirev8",
    srcs = ["wirev8.js"],
    deps = [
        ":wire",
        "//closure/goog/asserts",
        "//closure/goog/collections:maps",
        "//closure/goog/json",
        "//closure/goog/json:nativejsonprocessor",
        "//closure/goog/string:parser",
        "//closure/goog/structs",
    ],
)

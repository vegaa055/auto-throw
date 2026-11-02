#pragma once

/** Parameter IDs shared by the processor and editor. Never rename — they are
    the keys in saved sessions and host automation. */
namespace ParamIDs
{
    inline constexpr const char* autoDetect  = "auto";
    inline constexpr const char* manualThrow = "throw";

    inline constexpr const char* threshold   = "threshold";
    inline constexpr const char* gap         = "gap";
    inline constexpr const char* every       = "every";

    inline constexpr const char* length      = "length";
    inline constexpr const char* snap        = "snap";

    inline constexpr const char* time        = "time";
    inline constexpr const char* feedback    = "feedback";
    inline constexpr const char* lowCut      = "lowcut";
    inline constexpr const char* highCut     = "highcut";
    inline constexpr const char* pingPong    = "pingpong";

    inline constexpr const char* reverb      = "reverb";
    inline constexpr const char* size        = "size";

    inline constexpr const char* duck        = "duck";
    inline constexpr const char* duckRelease = "duckrelease";

    inline constexpr const char* wet         = "wet";
    inline constexpr const char* dry         = "dry";
}

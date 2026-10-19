#include "head_id.h"

namespace rt {

HeadType head_from_mv(int mv) {
    if (mv < kHeadFaultBelowMv) return HeadType::NONE;
    if (mv < kHeadThr24UwbMv) return HeadType::H24;
    if (mv < kHeadThrUwbDualMv) return HeadType::UWB;
    if (mv < kHeadThrDualNoneMv) return HeadType::DUAL;
    return HeadType::NONE;
}

const char* head_name(HeadType h) {
    switch (h) {
        case HeadType::NONE: return "none";
        case HeadType::H24: return "24GHz";
        case HeadType::UWB: return "UWB";
        case HeadType::DUAL: return "dual";
    }
    return "?";
}

}  // namespace rt

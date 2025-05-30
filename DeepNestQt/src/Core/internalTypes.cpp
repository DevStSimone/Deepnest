#include "internalTypes.h"
// No specific code needed yet if header only contains type definitions


namespace Core {

InternalPart::InternalPart(const InternalSheet& part )
    : outerBoundary(part.outerBoundary), holes(part.holes) {
    if (!outerBoundary.isEmpty()) {
        bounds = outerBoundary.boundingRect();
    }
}

}

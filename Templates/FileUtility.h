#ifndef FileUtility_h
#define FileUtility_h

#include <fstream>
#include "RawBuffer.h"

namespace file_utility{
    RawBuffer readFile(std::string const& filename);
}
#endif

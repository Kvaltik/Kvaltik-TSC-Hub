#pragma once
#include "../Core/Storage.h"
namespace Operations {
// Documents are local operational notes, never serialized as playable TSC scenarios.
inline Core::fs::path Document(const Core::fs::path& directory,int page){
    if(page==12)return directory/L"timetable.txt";
    if(page==13)return directory/L"orders.txt";
    return {};
}
}

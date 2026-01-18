// 2009 © Václav Šmilauer <eudoxos@arcig.cz>
#pragma once
#include"compat.hpp"

//! run string as python command; locks & unlocks GIL automatically
void pyRunString(const std::string& cmd);


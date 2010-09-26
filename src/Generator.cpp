/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#include "Internal.h"
#include "Generator.h"
#include "Parser.h"

elGenerator::elGenerator()
{
    return;
}

elGenerator::~elGenerator()
{
    return;
}

void elGenerator::Initialize()
{
    m_Streams.clear();
    return;
}

void elGenerator::AddFrameFromStream(const elFrame& Fr)
{
    m_Streams.push_back(Fr);
    return;
}

bool elGenerator::Generate(bsBitstream& OS)
{
    return true;
}

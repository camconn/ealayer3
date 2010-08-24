/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#include "Internal.h"
#include "SingleBlockLoader.h"
#include "../Parser.h"
#include "../Parsers/ParserVersion6.h"

elSingleBlockLoader::elSingleBlockLoader() :
    m_Compression(0)
{
    return;
}

elSingleBlockLoader::~elSingleBlockLoader()
{
    return;
}

const std::string elSingleBlockLoader::GetName() const
{
    return "Single Block Header";
}

bool elSingleBlockLoader::Initialize(std::istream* Input)
{
    elBlockLoader::Initialize(Input);

    const std::streamoff StartOffset = m_Input->tellg();

    // Read in some values
    uint8_t Compression;
    uint8_t ChannelValue;
    uint16_t SampleRate;
    uint32_t TotalSamples1;
    uint32_t BlockSize;
    uint32_t TotalSamples2;
    
    m_Input->read((char*)&Compression, 1);
    m_Input->read((char*)&ChannelValue, 1);
    m_Input->read((char*)&SampleRate, 2);
    m_Input->read((char*)&TotalSamples1, 4);
    m_Input->read((char*)&BlockSize, 4);
    m_Input->read((char*)&TotalSamples2, 4);

    Swap(SampleRate);
    Swap(TotalSamples1);
    Swap(BlockSize);
    Swap(TotalSamples2);

    // Make sure its valid
    if (Compression < 5 || Compression > 7)
    {
        VERBOSE("L: single block loader incorrect because of compression");
        return false;
    }
    m_Compression = Compression;
    
    if (ChannelValue % 4 != 0)
    {
        VERBOSE("L: single block loader incorrect because of channel value");
        return false;
    }
    if (TotalSamples1 != TotalSamples2)
    {
        VERBOSE("L: single block loader incorrect because total samples don't equal each other");
        return false;
    }
    m_Input->seekg(0, std::ios_base::end);
    if (BlockSize + 8 > m_Input->tellg())
    {
        VERBOSE("L: single block loader incorrect because of size");
        return false;
    }

    VERBOSE("L: single block loader correct");
    m_Input->clear();
    m_Input->seekg(StartOffset);
    return true;
}

bool elSingleBlockLoader::ReadNextBlock(elBlock& Block)
{
    if (m_Input->eof() || m_CurrentBlockIndex)
    {
        return false;
    }

    std::streamoff Offset = m_Input->tellg();

    // Read in some values
    uint8_t Compression;
    uint8_t ChannelValue;
    uint16_t SampleRate;
    uint32_t TotalSamples1;
    uint32_t BlockSize;
    uint32_t TotalSamples2;

    m_Input->read((char*)&Compression, 1);
    m_Input->read((char*)&ChannelValue, 1);
    m_Input->read((char*)&SampleRate, 2);
    m_Input->read((char*)&TotalSamples1, 4);
    m_Input->read((char*)&BlockSize, 4);
    m_Input->read((char*)&TotalSamples2, 4);

    Swap(SampleRate);
    Swap(TotalSamples1);
    Swap(BlockSize);
    Swap(TotalSamples2);

    // Now load the data
    BlockSize -= 8;

    shared_array<uint8_t> Data(new uint8_t[BlockSize]);
    m_Input->read((char*)Data.get(), BlockSize);

    Block.Clear();
    Block.Data = Data;
    Block.SampleCount = TotalSamples1;
    Block.Size = BlockSize;
    Block.Offset = Offset;

    m_CurrentBlockIndex++;
    return true;
}

shared_ptr<elParser> elSingleBlockLoader::CreateParser() const
{
    switch (m_Compression)
    {
        case 5:
            return make_shared<elParser>();
        case 6:
        case 7:
            return make_shared<elParserVersion6>();
    }
    return shared_ptr<elParser>();
}

void elSingleBlockLoader::ListSupportedParsers(std::vector< std::string >& Names) const
{
    Names.push_back(make_shared<elParser>()->GetName());
    Names.push_back(make_shared<elParserVersion6>()->GetName());
    return;
}

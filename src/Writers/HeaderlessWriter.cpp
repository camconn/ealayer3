/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#include "Internal.h"
#include "HeaderlessWriter.h"

/*
el::elHeaderlessLoader() :
        m_LastPacket(false)
{
    return;
}

elHeaderlessLoader::~elHeaderlessLoader()
{
    return;
}

const std::string elHeaderlessLoader::GetName() const
{
    return "Headerless";
}

bool elHeaderlessLoader::Initialize(std::istream* Input)
{
    elBlockLoader::Initialize(Input);

    // Save the starting offset
    const std::streamoff StartOffset = m_Input->tellg();

    for (unsigned int i = 0; i < 5 && !m_Input->eof(); i++)
    {
        uint16_t Flags;
        uint16_t BlockSize;
        uint32_t Samples;
        
        m_Input->read((char*)&Flags, 2);
        m_Input->read((char*)&BlockSize, 2);
        m_Input->read((char*)&Samples, 4);

        Swap(Flags);
        Swap(BlockSize);
        Swap(Samples);

        if (Flags & 0x8000)
        {
            break;
        }
        if (Flags & 0x7FFF)
        {
            VERBOSE("L: headerless loader incorrect because of flags");
            return false;
        }

        if (BlockSize < 8)
        {
            VERBOSE("L: headerless loader incorrect because block size < 8");
            return false;
        }

        BlockSize -= 8;

        m_Input->seekg(BlockSize, std::ios_base::cur);
    }

    m_Input->clear();
    m_Input->seekg(StartOffset);

    VERBOSE("L: headerless loader correct");
    m_LastPacket = false;
    return true;
}

bool elHeaderlessLoader::ReadNextBlock(elBlock& Block)
{
    if (!m_Input)
    {
        return false;
    }

    if (m_LastPacket || m_Input->eof())
    {
        return false;
    }

    uint16_t Flags;
    uint16_t BlockSize;
    uint32_t Samples;

    const std::streamoff Offset = m_Input->tellg();

    m_Input->read((char*)&Flags, 2);
    m_Input->read((char*)&BlockSize, 2);
    m_Input->read((char*)&Samples, 4);

    if (m_Input->eof())
    {
        return false;
    }

    Swap(Flags);
    Swap(BlockSize);
    Swap(Samples);

    if (Flags & 0x8000)
    {
        m_LastPacket = true;
    }

    if (BlockSize <= 8)
    {
        return false;
    }

    BlockSize -= 8;

    shared_array<uint8_t> Data(new uint8_t[BlockSize]);
    m_Input->read((char*)Data.get(), BlockSize);

    Block.Clear();
    Block.Data = Data;
    Block.SampleCount = Samples;
    Block.Size = BlockSize;
    Block.Offset = Offset;

    m_CurrentBlockIndex++;
    return true;
}

shared_ptr<elParser> elHeaderlessLoader::CreateParser() const
{
    shared_ptr<elParserSelector> Selector = make_shared<elParserSelector>();
    elParserSelector::fsFormat Formats[] = {
        make_shared<elParserVersion6>(),
        make_shared<elParser>()
    };

    Selector->SelectorListAdd(Formats, sizeof(Formats) / sizeof(elParserSelector::fsFormat));
    return Selector;
}

void elHeaderlessLoader::ListSupportedParsers(std::vector< std::string >& Names) const
{
    Names.push_back(make_shared<elParser>()->GetName());
    Names.push_back(make_shared<elParserVersion6>()->GetName());
    return;
}

*/

elHeaderlessWriter::elHeaderlessWriter()
{
    return;
}

elHeaderlessWriter::~elHeaderlessWriter()
{
    return;
}

void elHeaderlessWriter::Initialize(std::ostream* Output)
{
    if (!Output)
    {
        return;
    }
    elBlockWriter::Initialize(Output);

    // No header. Otherwise it would be written here.
    return;
}

void elHeaderlessWriter::WriteNextBlock(const elBlock& Block, bool LastBlock)
{
    // Sanity check
    if (!m_Output)
    {
        return;
    }

    // Calculate the variables
    uint16_t Flags;
    uint16_t BlockSize;
    uint32_t Samples;

    Flags = LastBlock ? 0x8000 : 0x0000;
    BlockSize = Block.Size + 8;
    Samples = Block.SampleCount;

    // Swap
    Swap(Flags);
    Swap(BlockSize);
    Swap(Samples);

    // Write
    m_Output->write((char*)&Flags, 2);
    m_Output->write((char*)&BlockSize, 2);
    m_Output->write((char*)&Samples, 4);
    m_Output->write((char*)Block.Data.get(), Block.Size);
    return;
}

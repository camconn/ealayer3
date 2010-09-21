/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#include "Internal.h"
#include "MpegParser.h"
#include "Parser.h"
#include "Bitstream.h"
#include "MpegGenerator.h"

static const unsigned int MpegSampleRateTable[4][4] = {
    {11025, 12000, 8000, 0},
    {0, 0, 0, 0},
    {22050, 24000, 16000, 0},
    {44100, 48000, 32000, 0}
};

elMpegParser::elMpegParser()
{
    return;
}

elMpegParser::~elMpegParser()
{
    return;
}

void elMpegParser::Initialize(std::istream* Input)
{
    assert(Input);
    m_Input = Input;

    // TODO: Make sure this really is an MP3 file
    return;
}

bool elMpegParser::ReadFrame(elFrame& Frame)
{
    assert(m_Input);

    // Are there any frames left?
    if (!FramesLeft())
    {
        return false;
    }

    // Figure out what kind of frame this is
    uint8_t FrameHeader[10];
    std::streamoff StartOffset;
    StartOffset = m_Input->tellg();
    m_Input->read((char*)FrameHeader, 10);
    m_Input->seekg(StartOffset);

    // Based on what this is, process it.
    if (memcmp(FrameHeader, "ID3", 3) == 0)
    {
        SkipID3Tag(FrameHeader);
    }
    else if (FrameHeader[0] == 0xFF)
    {
        //m_Input->seekg(StartOffset + 4);
        ProcessFrameHeader(Frame, FrameHeader);
        // TODO: Check if this is really a frame or just VBR info
    }
    return false;
}

bool elMpegParser::FramesLeft() const
{
    assert(m_Input);
    return !m_Input->eof();
}

void elMpegParser::SkipID3Tag(uint8_t FrameHeader[10])
{
    // Read the number from the frame header
    uint8_t TempNumber[4];
    bsBitstream IS(FrameHeader, 10);
    bsBitstream Temp(TempNumber, 4);
    IS.SeekAbsolute(6 * 8);
    Temp.WriteBits(0, 4);

    for (unsigned int i = 0; i < 4; i++)
    {
        IS.ReadBit();
        Temp.WriteBits(IS.ReadBits(7), 7);
    }
    Temp.WriteToNextByte();

    // Get the size
    unsigned int Size;
    Temp.SeekAbsolute(0);
    Size = Temp.ReadAligned32BE<unsigned int>();

    VERBOSE("ID3 Tag size: " << Size);

    // Finally seek past it.
    m_Input->seekg(Size + 10, std::ios_base::cur);
    return;
}

bool elMpegParser::ProcessFrameHeader(elFrame& Frame, uint8_t FrameHeader[10])
{
    bsBitstream IS(FrameHeader, 10);
    
    // Read the fields
    if (IS.ReadBits(11) != 0x7FF)
    {
        throw (elMpegParserException("MPEG sync bits don't match. Keep in mind that for this program to work the MP3 must be well-formed."));
    }
    
    Frame.Gr[0].Version = Frame.Gr[1].Version = IS.ReadBits(2);
    
    if (IS.ReadBits(2) != 0x1)
    {
        throw (elMpegParserException("File not supported; only MPEG layer 3 is supported."));
    }
    
    bool Crc = IS.ReadBit() == 1 ? false : true;
    
    unsigned int BitrateIndex = IS.ReadBits(4);
    
    Frame.Gr[0].SampleRateIndex = Frame.Gr[1].SampleRateIndex = IS.ReadBits(2);
    
    bool Padding = IS.ReadBit() == 0 ? false : true;
    
    IS.ReadBit();
    
    Frame.Gr[0].ChannelMode = Frame.Gr[1].ChannelMode = IS.ReadBits(2);
    
    Frame.Gr[0].ModeExtension = Frame.Gr[1].ModeExtension = IS.ReadBits(2);
    
    IS.ReadBits(4);

    // Sample rate
    Frame.Gr[0].SampleRate = Frame.Gr[1].SampleRate =
        MpegSampleRateTable[Frame.Gr[0].Version][Frame.Gr[0].SampleRateIndex];

    // Calculate the total size
    unsigned int FrameSize;
    FrameSize = elMpegGenerator::CalculateFrameSize(BitrateIndex, Frame.Gr[0].SampleRate,
                                                    Frame.Gr[0].Version);

    if (Padding)
    {
        FrameSize++;
    }

    // Check to see if this is a VBR frame

    // Read the side info

    // Get the bits from the reservoir

    // Get the main data

    // Put the bits on the end into the reservoir

    m_Input->seekg(FrameSize, std::ios_base::cur);
    
    VERBOSE("Frame size: " << FrameSize);
    return true;
}


elMpegParserException::elMpegParserException(const std::string& What) throw() :
    m_What(What)
{
    return;
}

elMpegParserException::~elMpegParserException() throw()
{
    return;
}

const char* elMpegParserException::what() const throw()
{
    return m_What.c_str();
}

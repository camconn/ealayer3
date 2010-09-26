/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#include "Internal.h"

#include <fstream>
#include <boost/format.hpp>

#include "Version.h"
#include "AllFormats.h"
#include "MpegGenerator.h"
#include "MpegOutputStream.h"
#include "PcmOutputStream.h"
#include "WaveWriter.h"
#include "Parsers/ParserVersion6.h"

#include "MpegParser.h"
#include "Generator.h"
#include "BlockWriter.h"

int g_Verbose = 0;

enum EOutputFormat
{
    EOF_AUTO,
    EOF_MP3,
    EOF_WAVE,
    EOF_MULTI_WAVE,
    EOF_EALAYER3
};

enum EParser
{
    EP_AUTO,
    EP_VERSION5,
    EP_VERSION6
};

struct SArguments
{
    SArguments() :
        ShowBanner(true),
        ShowUsage(false),
        ShowInfo(false),
        Parser(EP_AUTO),
        InputFilename(""),
        OutputFilename(""),
        StreamIndex(0),
        AllStreams(false),
        Offset(0),
        OutputFormat(EOF_AUTO)
    {
    };

    bool ShowBanner;
    bool ShowUsage;
    bool ShowInfo;
    EParser Parser;
    std::string InputFilename;
    std::string OutputFilename;
    unsigned int StreamIndex;
    bool AllStreams;
    std::streamoff Offset;
    EOutputFormat OutputFormat;

    std::vector<std::string> InputFilenameVector;
};

// Functions in this file
void SeparateFilename(const std::string& Filename, std::string& PathAndName, std::string& Ext);
bool ParseArguments(SArguments& Args, unsigned long Argc, char* Argv[]);
void ShowUsage(const std::string& Program);
void SetOutputFormat(SArguments& Args);
void CreateOutputFilename(SArguments& Args);
bool OpenOutputFile(std::ofstream& Output, const std::string& Filename);
void FinishParsingBlocks(elBlockLoader& Loader, elMpegGenerator& Gen);
void OutputMultiChannel(std::ofstream& Output, elMpegGenerator& Gen, SArguments& Args);
int Encode(SArguments& Args);


void SeparateFilename(const std::string& Filename, std::string& PathAndName, std::string& Ext)
{
    PathAndName = Filename;
    Ext = "";

    for (unsigned int i = Filename.length(); i > 0; i--)
    {
        if (Filename.at(i - 1) == '.')
        {
            PathAndName = Filename.substr(0, i - 1);
            Ext = Filename.substr(i - 1);
            break;
        }
    }

    return;
}

bool ParseArguments(SArguments& Args, unsigned long Argc, char* Argv[])
{
    for (unsigned int i = 1; i < Argc;)
    {
        std::string Arg(Argv[i++]);

        // Check it

        if (Arg == "-b-" || Arg == "--no-banner")
        {
            Args.ShowBanner = false;
        }
        else if (Arg == "-o" || Arg == "--output")
        {
            if (i >= Argc)
            {
                return false;
            }

            Args.OutputFilename = std::string(Argv[i++]);
        }
        else if (Arg == "-s" || Arg == "--stream")
        {
            if (i >= Argc)
            {
                return false;
            }

            if (std::string(Argv[i]) == "all")
            {
                Args.AllStreams = true;
                Args.StreamIndex = 0;
                i++;
            }
            else
            {
                Args.StreamIndex = atoi(Argv[i++]) - 1;
                Args.AllStreams = false;
            }
        }
        else if (Arg == "-i" || Arg == "--offset")
        {
            if (i >= Argc)
            {
                return false;
            }

            Args.Offset = atoi(Argv[i++]);
        }
        else if (Arg == "-n" || Arg == "--info")
        {
            Args.ShowInfo = true;
        }
        else if (Arg == "-w" || Arg == "--wave")
        {
            Args.OutputFormat = EOF_WAVE;
        }
        else if (Arg == "-mc" || Arg == "--multi-wave")
        {
            Args.OutputFormat = EOF_MULTI_WAVE;
        }
        else if (Arg == "-m" || Arg == "--mp3")
        {
            Args.OutputFormat = EOF_MP3;
        }
        else if (Arg == "-v" || Arg == "--verbose")
        {
            g_Verbose = 1;
        }
        else if (Arg == "--parser5")
        {
            Args.Parser = EP_VERSION5;
        }
        else if (Arg == "--parser6")
        {
            Args.Parser = EP_VERSION6;
        }
        else if (Arg == "-E")
        {
            Args.OutputFormat = EOF_EALAYER3;
        }
        else
        {
            Args.InputFilename = Arg;
            Args.InputFilenameVector.push_back(Arg);
        }
    }

    return true;
}

void ShowUsage(const std::string& Program)
{
    std::cout << "Usage: " << Program << " InputFilename [Options]" << std::endl;
    std::cout << std::endl;
    std::cout << "  -i, --offset Offset   Specify the offset in the file to begin at." << std::endl;
    std::cout << "  -o, --output File     Specify the output filename (.mp3)." << std::endl;
    std::cout << "  -s, --stream Index    Specify which stream to extract, or all." << std::endl;
    std::cout << "  -m, --mp3             Output to MP3 (no information loss!)." << std::endl;
    std::cout << "  -w, --wave            Output to Microsoft WAV." << std::endl;
    std::cout << "  -mc, --multi-wave     Output to a multi-channel Microsoft WAV." << std::endl;
    std::cout << "  --parser5             Force using the version 5 parser." << std::endl;
    std::cout << "  -n, --info            Output information about the file." << std::endl;
    std::cout << "  -v, --verbose         Be verbose (useful when streams won't convert)." << std::endl;
    std::cout << "  -b-, --no-banner      Don't show the banner." << std::endl;
    std::cout << std::endl;
    std::cout << "Supported formats: " << std::endl;

    // List supported formats
    elBlockLoaderSelector Loader;

    for (elBlockLoaderSelector::fsFormatList::const_iterator Fmt = Loader.SelectorList().begin();
            Fmt != Loader.SelectorList().end(); ++Fmt)
    {
        std::cout << "   * " << (*Fmt)->GetName() << std::endl;

        // Get supported parsers
        std::vector<std::string> Parsers;
        (*Fmt)->ListSupportedParsers(Parsers);

        for (std::vector<std::string>::const_iterator Parser = Parsers.begin();
                Parser != Parsers.end(); ++Parser)
        {
            std::cout << "      - " << (*Parser) << std::endl;
        }
    }

    std::cout << std::endl;

    return;
}

int main(int Argc, char **Argv)
{
    // Parse the arguments
    SArguments Args;
    bool ArgParse;

    if (Argc == 1)
    {
        Args.ShowUsage = true;
    }

    ArgParse = ParseArguments(Args, Argc, Argv);

    // Display banner
    if (Args.ShowBanner)
    {
        std::cerr << "EA Layer 3 Stream Extractor/Decoder ";
        std::cerr << ealayer3_VERSION_MAJOR << "." << ealayer3_VERSION_MINOR << "." << ealayer3_VERSION_PATCH;
        std::cerr << ". Copyright (C) 2010, Ben Moench." << std::endl;
        std::cerr << std::endl;
    }

    // Display usage
    if (Args.ShowUsage)
    {
        ShowUsage(Argv[0]);
    }

    // Check for errors
    if (!ArgParse)
    {
        std::cerr << "The arguments are not valid." << std::endl;
        return 1;
    }

    if (Args.InputFilename.empty())
    {
        std::cerr << "You must specify an input filename." << std::endl;
        return 1;
    }

    // Set the output format if not already set
    if (Args.OutputFormat == EOF_AUTO)
    {
        SetOutputFormat(Args);
    }

    if (Args.OutputFormat == EOF_EALAYER3)
    {
        return Encode(Args);
    }

    // Create an output filename if there isn't already one
    bool ShowOutputFile = false;

    if (Args.OutputFilename.empty() && !Args.ShowInfo)
    {
        CreateOutputFilename(Args);
        ShowOutputFile = true;
    }

    // Open the input file
    std::ifstream Input;

    Input.open(Args.InputFilename.c_str(), std::ios_base::in | std::ios_base::binary);

    if (!Input.is_open())
    {
        std::cerr << "Could not open input file '" << Args.InputFilename << "'." << std::endl;
        return 1;
    }

    // Determine the input's file type here
    elBlockLoaderSelector Loader;

    Input.seekg(Args.Offset);

    if (!Loader.Initialize(&Input))
    {
        std::cerr << "The input is not in a readable file format." << std::endl;
        return 1;
    }

    // Grab the first block
    elMpegGenerator Gen;

    elBlock FirstBlock;

    if (!Loader.ReadNextBlock(FirstBlock))
    {
        std::cerr << "The first block could not be read from the input." << std::endl;
        return 1;
    }

    // Create the parser.
    shared_ptr<elParser> Parser;
    switch (Args.Parser)
    {
    case EP_VERSION5:
        Parser = make_shared<elParser>();
        break;

    case EP_VERSION6:
        Parser = make_shared<elParserVersion6>();
        break;
        
    case EP_AUTO:
    default:
        Parser = Loader.CreateParser();
        break;
    }

    if (!Gen.Initialize(FirstBlock, Parser))
    {
        std::cerr << "The EALayer3 parser could not be initialized (the bitstream format is not readable)." << std::endl;
        return 1;
    }

    // Check the stream index
    if (Args.StreamIndex >= Gen.GetStreamCount())
    {
        std::cerr << "The stream index (" << Args.StreamIndex << ") exceeds the total number of streams (" << Gen.GetStreamCount() << ")." << std::endl;
        return 1;
    }

    // Show the info
    if (Args.ShowInfo)
    {
        std::cout << "Stream count: " << Gen.GetStreamCount() << std::endl;
        std::cout << std::endl;

        if (Args.OutputFilename.empty())
        {
            return 0;
        }
    }

    if (Args.AllStreams && Gen.GetStreamCount() > 32)
    {
        std::cerr << "Too many streams to be decoded." << std::endl;
        return 1;
    }

    // Open the output files
    std::ofstream OutputArray[32];

    unsigned int OutputCount = 0;

    if (Args.AllStreams && Gen.GetStreamCount() > 1 && Args.OutputFormat != EOF_MULTI_WAVE)
    {
        // Get the base name
        std::string PathAndName;
        std::string Ext;

        SeparateFilename(Args.OutputFilename, PathAndName, Ext);

        for (unsigned int i = 0; i < Gen.GetStreamCount(); i++)
        {
            std::string OutputName;
            OutputName = (boost::format("%s_%u%s") % PathAndName % (i + 1) % Ext).str();

            if (ShowOutputFile || Args.ShowInfo)
            {
                std::cout << "Output: " << OutputName << std::endl;
            }

            if (!OpenOutputFile(OutputArray[OutputCount++], OutputName))
            {
                return 1;
            }
        }
    }
    else
    {
        if (ShowOutputFile || Args.ShowInfo)
        {
            std::cout << "Output: " << Args.OutputFilename << std::endl;
        }

        if (!OpenOutputFile(OutputArray[OutputCount++], Args.OutputFilename))
        {
            return 1;
        }
    }

    // Make sure we have something to decode
    if (OutputCount == 0)
    {
        std::cerr << "Nothing was decoded." << std::endl;
    }
    else if (OutputCount > Gen.GetStreamCount())
    {
        std::cerr << "Program bug: Outputs.size() > Parser.GetStreamCount()" << std::endl;
        return 1;
    }

    // Do it!
    try
    {
        // Load in the file
        VERBOSE("Parsing blocks...");
        Gen.ParseBlock(FirstBlock);
        FinishParsingBlocks(Loader, Gen);

        // Write it out in the preferred output format
        VERBOSE("Writing output file...");

        if (Args.OutputFormat == EOF_MULTI_WAVE)
        {
            OutputMultiChannel(OutputArray[0], Gen, Args);
        }
        else
        {
            for (unsigned int i = 0; i < OutputCount; i++)
            {
                const unsigned int MpegBufferSize = MAX_MPEG_FRAME_BUFFER;
                shared_array<uint8_t> MpegBuffer(new uint8_t[MpegBufferSize]);
                const unsigned int PcmBufferSamples = elPcmOutputStream::RecommendBufferSize();
                shared_array<short> PcmBuffer(new short[PcmBufferSamples]);

                if (Args.OutputFormat == EOF_MP3)
                {
                    shared_ptr<elMpegOutputStream> Stream = Gen.CreateMpegStream(Args.StreamIndex + i);

                    while (!Stream->Eos())
                    {
                        unsigned int Read;
                        Read = Stream->Read(MpegBuffer.get(), MpegBufferSize);
                        OutputArray[i].write((char*) MpegBuffer.get(), Read);
                    }
                }
                else if (Args.OutputFormat == EOF_WAVE)
                {
                    shared_ptr<elPcmOutputStream> Stream = Gen.CreatePcmStream(Args.StreamIndex + i);
                    PrepareWaveHeader(OutputArray[i]);

                    while (!Stream->Eos())
                    {
                        unsigned int Read;
                        Read = Stream->Read(PcmBuffer.get(), PcmBufferSamples);
                        OutputArray[i].write((char*) PcmBuffer.get(), Read * sizeof(short));
                    }

                    const unsigned int SampleCount = ((unsigned int) OutputArray[i].tellp() - 44) / 2;

                    OutputArray[i].seekp(0);

                    WriteWaveHeader(OutputArray[i], Gen.GetSampleRate(Args.StreamIndex + i), 16,
                                    Gen.GetChannels(Args.StreamIndex + i), SampleCount);
                }
            }
        }
    }
    catch (elParserException& E)
    {
        std::cerr << "Problems reading the input file." << std::endl;
        std::cerr << "Exception: " << E.what() << std::endl;
        return 1;
    }
    catch (std::exception& E)
    {
        std::cerr << "There was an error." << std::endl;
        std::cerr << "Exception: " << E.what() << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cerr << "Crashed." << std::endl;
        return 1;
    }

    // Write out some information
    if (Args.ShowInfo)
    {
        Input.clear();

        std::cout << "Uncompressed sample frames: " << Gen.GetUncSampleFrameCount() << std::endl;
        std::cout << "End offset in file: " << Input.tellg() << std::endl;
    }

    VERBOSE("Done.");

    return 0;
}

void SetOutputFormat(SArguments& Args)
{
    // Autodetect the output format from filename
    if (Args.OutputFilename.empty())
    {
        Args.OutputFormat = EOF_MP3;
    }
    else
    {
        std::string PathAndName;
        std::string Ext;
        SeparateFilename(Args.OutputFilename, PathAndName, Ext);

        for (unsigned int i = 0; i < Ext.length(); i++)
        {
            Ext[i] = tolower(Ext[i]);
        }

        if (Ext == ".mp3")
        {
            Args.OutputFormat = EOF_MP3;
        }
        else if (Ext == ".wav")
        {
            Args.OutputFormat = EOF_WAVE;
        }
        else
        {
            Args.OutputFormat = EOF_MP3;
        }
    }

    return;
}

void CreateOutputFilename(SArguments& Args)
{
    std::string PathAndName;
    std::string Ext;

    SeparateFilename(Args.InputFilenameVector[0], PathAndName, Ext);
    Args.OutputFilename = PathAndName;

    switch (Args.OutputFormat)
    {

    case EOF_MP3:
        Args.OutputFilename.append(".mp3");
        break;

    case EOF_WAVE:
        Args.OutputFilename.append(".wav");
        break;

    case EOF_MULTI_WAVE:
        Args.OutputFilename.append(".wav");
        break;

    case EOF_EALAYER3:
        Args.OutputFilename.append(".ealayer3");
        break;
    }

    return;
}

bool OpenOutputFile(std::ofstream& Output, const std::string& Filename)
{
    Output.open(Filename.c_str(), std::ios_base::out | std::ios_base::binary);

    if (!Output.is_open())
    {
        std::cerr << "Could not open output file '" << Filename << "'." << std::endl;
        return false;
    }

    return true;
}

void FinishParsingBlocks(elBlockLoader& Loader, elMpegGenerator& Gen)
{
    while (true)
    {
        elBlock Block;

        if (!Loader.ReadNextBlock(Block))
        {
            break;
        }

        Gen.ParseBlock(Block);
    }

    Gen.DoneParsingBlocks();

    return;
}

void OutputMultiChannel(std::ofstream& Output, elMpegGenerator& Gen, SArguments& Args)
{
    // Create the streams
    std::vector< shared_ptr<elPcmOutputStream> > Streams;
    unsigned int ChannelCount = 0;

    for (unsigned int i = 0; i < Gen.GetStreamCount(); i++)
    {
        Streams.push_back(Gen.CreatePcmStream(i));
        ChannelCount += Gen.GetChannels(i);
    }

    if (!Streams.size())
    {
        return;
    }

    // Allocate a buffer
    shared_array<short> ReadBuffer(new short[ChannelCount *
                                   elPcmOutputStream::RecommendBufferSize()]);
    const unsigned int PcmBufferSamples = elPcmOutputStream::RecommendBufferSize();
    shared_array<short> PcmBuffer(new short[PcmBufferSamples]);

    // Prepare the wave
    PrepareWaveHeader(Output);

    // Decode
    while (!Streams[0]->Eos())
    {
        unsigned int Ch = 0;
        unsigned int Frames = 0;

        for (unsigned int i = 0; i < Gen.GetStreamCount(); i++)
        {
            unsigned int Read;
            Read = Streams[i]->Read(PcmBuffer.get(), PcmBufferSamples);
            Frames = Read / Streams[i]->GetChannels();

            unsigned int OldCh = Ch;

            for (unsigned int j = 0; j < Frames; j++)
            {
                Ch = OldCh;

                for (unsigned int k = 0; k < Streams[i]->GetChannels(); k++)
                {
                    ReadBuffer[j * ChannelCount + Ch] = PcmBuffer[j * Streams[i]->GetChannels() + k];
                    Ch++;
                }
            }
        }

        Output.write((char*) ReadBuffer.get(), Frames * ChannelCount * sizeof(short));
    }

    const unsigned int SampleCount = ((unsigned int) Output.tellp() - 44) / 2;
    Output.seekp(0);
    WriteWaveHeader(Output, Gen.GetSampleRate(0), 16,
                    ChannelCount, SampleCount);

    return;
}

struct elEncodeInput
{
    elEncodeInput(const std::string& MpegFile, const std::string& WaveFile) :
        MpegFile(MpegFile), WaveFile(WaveFile) {}
        
    std::string MpegFile;
    std::string WaveFile;

    shared_ptr<std::ifstream> MpegInput;
    shared_ptr<std::ifstream> WaveInput;

    shared_ptr<elMpegParser> MpegParser;

    // MpegParser? WaveParser?
};

int Encode(SArguments& Args)
{
    // Create an output filename if there isn't already one
    bool ShowOutputFile = false;

    if (Args.OutputFilename.empty() && !Args.ShowInfo)
    {
        CreateOutputFilename(Args);
        ShowOutputFile = true;
    }

    // TODO: Parse the input parameters
    std::vector<elEncodeInput> InputFiles;
    InputFiles.push_back(elEncodeInput(Args.InputFilenameVector[0], ""));

    // Create an MpegParser and perhaps a WaveParser for each of the inputs
    shared_ptr<std::ifstream> Input = make_shared<std::ifstream>();
    Input->open(InputFiles.back().MpegFile.c_str());
    if (!Input->is_open())
    {
        std::cerr << "Could not open input file '" << InputFiles.back().MpegFile.c_str() << "'." << std::endl;
        return 1;
    }
    InputFiles.back().MpegInput = Input;

    // Create the parser
    shared_ptr<elMpegParser> Parser = make_shared<elMpegParser>();
    InputFiles.back().MpegParser = Parser;
    Parser->Initialize(Input.get());

    // Open output file
    std::ofstream Output;
    if (!OpenOutputFile(Output, Args.OutputFilename))
    {
        return 1;
    }

    elGenerator Gen;

    elFrame Frame;
    for (unsigned int i = 0; i < 10; i++)
    {
        Parser->ReadFrame(Frame);
    }
    return 0;
}

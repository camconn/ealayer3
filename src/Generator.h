/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#pragma once

#include "Internal.h"

class elFrame;
class bsBitstream;

/**
 * An EA Layer 3 generator.
 */
class elGenerator
{
public:
    elGenerator();
    virtual ~elGenerator();

    /**
     * Initialize the generator.
     */
    virtual void Initialize();

    /**
     * Push a frame onto the queue. For each frame this should be called once
     * for each stream, and after that Generate should be called to create the
     * resulting block.
     */
    virtual void AddFrameFromStream(const elFrame& Fr);

    /**
     * Generate the resulting block. Takes all of the frames pushed so far, and
     * then clears the queue.
     */
    virtual bool Generate(bsBitstream& OS);

protected:
    std::vector<elFrame> m_Streams;
};

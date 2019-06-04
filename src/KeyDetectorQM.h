/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/*
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef KEY_DETECTOR_QM_H
#define KEY_DETECTOR_QM_H

#include "KeyDetectorIface.h"
#include <vector>

class Decimator;
class Chromagram;

namespace KD {

class KeyDetectorQM : public KeyDetectorIface
{
public:
    struct Config {
        double sampleRate;
        double tuningFrequency;
        int hpcpAverageWindowLength;
        int medianAverageWindowLength;

        Config(double _sampleRate) :
            sampleRate(_sampleRate),
            tuningFrequency(440.0),
            hpcpAverageWindowLength(10),
            medianAverageWindowLength(10) {
        }
    };
    
    KeyDetectorQM(Config config);

    virtual ~KeyDetectorQM();

    /**
     * Process a single time-domain input sample frame of length
     * getBlockSize(). Successive calls should provide overlapped data
     * with an advance of getHopSize() between frames.
     *
     * Return a key index in the range 0-24, where 0 indicates no key
     * detected, 1 is C major, and 13 is C minor.
     */
    virtual int process(double *frame);

    /**
     * Return a 24-element vector containing the correlation of the
     * chroma vector generated in the last process() call against the
     * stored key profiles for the 12 major and 12 minor keys, where
     * index 0 is C major and 12 is C minor.
     */
    virtual std::vector<double> getKeyStrengths() const;

    virtual int getHopSize() const;
    virtual int getBlockSize() const;

private:
    double krumCorr(const double *pDataNorm, const double *pProfileNorm, 
                    int shiftProfile, int length) const;

    double m_hpcpAverage;
    double m_medianAverage;
    int m_decimationFactor;

    // Decimator (fixed)
    Decimator *m_decimator;

    // Chromagram object
    Chromagram *m_chroma;

    // Chromagram output pointer
    double *m_chrPointer;

    int m_chromaFrameSize;
    int m_chromaHopSize;

    int m_chromaBufferSize;
    int m_medianWinSize;
        
    int m_bufferIndex;
    int m_chromaBufferFilling;
    int m_medianBufferFilling;

    double *m_decimatedBuffer;
    double *m_chromaBuffer;
    double *m_meanHPCP;

    double *m_majProfileNorm;
    double *m_minProfileNorm;
    double *m_majCorr;
    double *m_minCorr;
    int *m_medianFilterBuffer;
    int *m_sortedBuffer;
};

}

#endif

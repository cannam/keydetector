/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/*
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef KEY_DETECTOR_IFACE_H
#define KEY_DETECTOR_IFACE_H

#include <vector>

namespace KD {

class KeyDetectorIface
{
public:
    virtual ~KeyDetectorIface() { }
    
    /**
     * Process a single time-domain input sample frame of length
     * getBlockSize(). Successive calls should provide overlapped data
     * with an advance of getHopSize() between frames.
     *
     * Return a key index in the range 0-24, where 0 indicates no key
     * detected, 1 is C major, and 13 is C minor.
     */
    virtual int process(double *frame) = 0;

    /**
     * Return a 24-element vector containing the correlation of the
     * chroma vector generated in the last process() call against the
     * stored key profiles for the 12 major and 12 minor keys, where
     * index 0 is C major and 12 is C minor.
     */
    virtual std::vector<double> getKeyStrengths() const = 0;

    virtual int getHopSize() const = 0;
    virtual int getBlockSize() const = 0;
};

}

#endif

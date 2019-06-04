/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/*
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "keydetector/KeyDetector.h"

#include "KeyDetectorQM.h"
#include "KeyDetectorDaschuer.h"

#include <stdexcept>

namespace KD {

KeyDetector::KeyDetector(Config config) :
    m_kdi(0)
{
    switch (config.method) {

    case METHOD_QM: {
        KeyDetectorQM::Config qconfig(config.sampleRate);
        qconfig.tuningFrequency = config.tuningFrequency;
        qconfig.hpcpAverageWindowLength = config.smoothingWindowLength;
        qconfig.medianAverageWindowLength = config.smoothingWindowLength;
        m_kdi = new KeyDetectorQM(qconfig);
        break;
    }

    case METHOD_DASCHUER: {
        KeyDetectorDaschuer::Config dconfig(config.sampleRate);
        dconfig.tuningFrequency = config.tuningFrequency;
        dconfig.hpcpAverageWindowLength = config.smoothingWindowLength;
        dconfig.medianAverageWindowLength = config.smoothingWindowLength;
        m_kdi = new KeyDetectorDaschuer(dconfig);
        break;
    }

    default:
        throw std::logic_error("unknown config.method");
    }
}

KeyDetector::~KeyDetector()
{
    delete m_kdi;
}

int KeyDetector::process(double *pcmData)
{
    return m_kdi->process(pcmData);
}

int
KeyDetector::getHopSize() const
{
    return m_kdi->getHopSize();
}

int
KeyDetector::getBlockSize() const
{
    return m_kdi->getBlockSize();
}

std::vector<double>
KeyDetector::getKeyStrengths() const
{
    return m_kdi->getKeyStrengths();
}

}

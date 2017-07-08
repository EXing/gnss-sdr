/*!
 * \file observables_interface.h
 * \brief This class represents an interface to an Observables block.
 * \author Javier Arribas, 2011. jarribas(at)cttc.es
 *
 * Abstract class for Observables modules. Since all its methods are virtual,
 * this class cannot be instantiated directly, and a subclass can only be
 * instantiated directly if all inherited pure virtual methods have been
 * implemented by that class or a parent class.
 *
 * -------------------------------------------------------------------------
 *
 * Copyright (C) 2010-2015  (see AUTHORS file for a list of contributors)
 *
 * GNSS-SDR is a software defined Global Navigation
 *          Satellite Systems receiver
 *
 * This file is part of GNSS-SDR.
 *
 * GNSS-SDR is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GNSS-SDR is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNSS-SDR. If not, see <http://www.gnu.org/licenses/>.
 *
 * -------------------------------------------------------------------------
 */


#ifndef GNSS_SDR_OBSERVABLES_INTERFACE_H_
#define GNSS_SDR_OBSERVABLES_INTERFACE_H_

#include "gnss_block_interface.h"

/*!
 * \brief This abstract class represents an interface to an observables block.
 *
 * Abstract class for pseudorange_intefaces, derived from GNSSBlockInterface.
 * Since all its methods are virtual,
 * this class cannot be instantiated directly, and a subclass can only be
 * instantiated directly if all inherited pure virtual methods have been
 * implemented by that class or a parent class.
 */
class ObservablesInterface : public GNSSBlockInterface {
public:
    virtual void reset() = 0;
};

#endif /* GNSS_SDR_OBSERVABLES_INTERFACE_H_ */

/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Authors: 
Michael Berg <michael.berg@zalf.de>

Maintainers: 
Currently maintained by the authors.

This file is part of the MONICA model. 
Copyright (C) Leibniz Centre for Agricultural Landscape Research (ZALF)
*/

#ifndef MONICA_EXPORTS_H
#define MONICA_EXPORTS_H

#ifdef MONICA_EXPORTS
#define MONICA_API __declspec(dllexport)
#else
#define MONICA_API __declspec(dllimport)
#endif

#endif // MONICA_EXPORTS_H

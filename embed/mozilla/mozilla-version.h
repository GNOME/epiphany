/*
 *  Copyright (C) 2004 Crispin Flowerday
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#ifndef MOZILLA_VERSION_H
#define MOZILLA_VERSION_H

/**
 * Create a version int from components
 */
#define VERSION4(a,b,c,d) ((a << 24) + (b << 16) + (c << 8) + d)

/**
 * Macros for comparing mozilla version numbers
 */
#define MOZILLA_ALPHA   1
#define MOZILLA_BETA    2
#define MOZILLA_RC      3
#define MOZILLA_RELEASE 4

#define MOZILLA_CHECK_VERSION4(major, minor, type, micro) \
          (VERSION4(MOZILLA_MAJOR, MOZILLA_MINOR, MOZILLA_TYPE, MOZILLA_MICRO) >= \
           VERSION4(major, minor, type, micro))

#define MOZILLA_CHECK_VERSION3(a,b,c) MOZILLA_CHECK_VERSION4(a,b,MOZILLA_RELEASE,c)
#define MOZILLA_CHECK_VERSION2(a,b) MOZILLA_CHECK_VERSION3(a,b,0)

/* Use the following:
 *
 *  1.4.1 -> MOZILLA_CHECK_VERSION3 (1,4,1)
 *  1.7   -> MOZILLA_CHECK_VERSION2 (1,7)
 *  1.8a1 -> MOZILLA_CHECK_VERSION4 (1,8,MOZILLA_ALPHA,1)
 *  1.7a  -> MOZILLA_CHECK_VERSION4 (1,7,MOZILLA_ALPHA,0)
 *  1.7rc2 -> MOZILLA_CHECK_VERSION4 (1,7,MOZILLA_RC, 2)
 */

#endif /* MOZILLA_VERSION_H */

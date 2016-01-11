Biometric Evaluation Framework
==============================
Biometric Evaluation Framework is a set of C++ classes, error codes, and design
patterns used to create a common environment to provide logging, data
management, error handling, and other functionality that is needed for many
applications used in the testing of biometric software.

The goals of Biometric Evaluation Framework include:

 * reduce the amount of I/O error handling implemented by applications;
 * provide standard interfaces for data management and logging;
 * remove the need for applications to handle low-level events from the 
   operating system;
 * provide time tracking and constraints for blocks of code;
 * reduce potential for memory errors and corruption;
 * simplify the use of parallel processing.

How to Build
------------
If all requirements have been met, building is as simple as:
```bash
make -C common/
```

### Build Configuration
The makefile understands several variables:

 * **`CC`**: C compiler;
 * **`CXX`**: C++ compiler;
 * **`MPICXX`**: OpenMPI compiler;
 * **`EXTRA_CXXFLAGS`**: Additional compile-time flags to **`$(CC)`**;
 * **`EXTRA_CXXFLAGS`**: Additional compile-time flags to **`$(CXX)`**;
 * **`EXTRA_LDFLAGS`**: Additional link-time flags to **`$(CXX)`**;
 * **`32`**: Force 32-bit compilation when set to `1`;
 * **`64`**: Force 64-bit compilation when set to `1`.

Requirements
------------
 * A C++ 2011 compiler:
	* `g++` >= 4.7
	* `clang++` >= 3.1
	* `icpc` >= 15.0

 * A supported operating system:
	* RHEL/CentOS 7.x
	* OS X >= 10.9
	* Cygwin 1.7.x
	
 * System packages (depending on desired modules, see below).

Other operating systems and compilers are likely to work as expected, but have
not been tested.
 
System Packages
---------------
Some modules require system packages that may not be installed by default on
all operating systems. Package names are listed below for RHEL/CentOS and OS X
(via [MacPorts](https://www.macports.org)). Other operating systems may use
similarly-named packages.

### CORE
| Name         | RHEL/CentOS           | MacPorts                     |
|:------------:|:---------------------:|:----------------------------:|
| OpenSSL      | `openssl-devel`       | n/a (uses OS X CommonCrypto) |

### IO
| Name         | RHEL/CentOS  | MacPorts |
|:------------:|:------------:|:--------:|
| Zlib         | `zlib-devel` | `zlib`   |

### RECORDSTORE
| Name         | RHEL/CentOS    | MacPorts  |
|:------------:|:--------------:|:---------:|
| Berkeley DB  | `libdb-devel`  | `db44`    |
| SQLite 3     | `sqlite-devel` | `sqlite3` |
| Zlib         | `zlib-devel`   | `zlib`    |

### IMAGE
| Name                                        | RHEL/CentOS           | MacPorts      |
|:-------------------------------------------:|:---------------------:|:-------------:|
| OpenJPEG 1.x                                | `openjpeg-devel`      | `libopenjpeg` |
| libjpeg                                     | `libjpeg-turbo-devel` | `jpeg`        |
| libpng                                      | `libpng-devel`        | `libpng`      |
| Zlib                                        | `zlib-devel`          | `zlib`        |

### MPIBASE, MPIDISTRIBUTOR, MPIRECEIVER
| Name         | RHEL/CentOS     | MacPorts  |
|:------------:|:---------------:|:---------:|
| Open MPI     | `openmpi-devel` | `openmpi` |

With MacPorts, you may need to select a different MPI group if you have more than one
installed:

`sudo port select mpi openmpi-mp-fortran`

### VIDEO
| Name                        | RHEL/CentOS | MacPorts       |
|:---------------------------:|:-----------:|:--------------:|
| [ffmpeg](http://ffmpeg.org) | n/a         | `ffmpeg-devel` |


#### NIST Biometric Image Software (NBIS)
[NBIS](http://nist.gov/itl/iad/ig/nbis.cfm) is supported under current versions
of RHEL/CentOS, Ubuntu, and OS X. The Framework repository contains a subset
of NBIS that is built from the top-level makefile. However, if there is a need
to use the _official_ NBIS, then the makefile in `common/src/libbiomeval` can
changed to use that NBIS build. Biometric Evaluation Framework will look for NBIS
to be installed at `/usr/local/nbis`. To build NBIS,
[download the source](http://nist.gov/itl/iad/ig/nigos.cfm),
and follow this basic build procedure:

```
./setup.sh /usr/local/nbis [--without-X11]
make config it
sudo make install
```

---

As Seen In...
-------------
NIST is committed to using Biometric Evaluation Framework in their biometric
evaluations, including:

 * [FpVTE 2012](http://www.nist.gov/itl/iad/ig/fpvte2012.cfm);
 * [MINEX III](http://www.nist.gov/itl/iad/ig/minexiii.cfm);
 * [IREX III](http://www.nist.gov/itl/iad/ig/irexiii.cfm);
 * [FaCE](http://www.nist.gov/itl/iad/ig/facechallenges.cfm);
 * [FIVE](http://www.nist.gov/itl/iad/ig/five.cfm);
 * ...and more.

Communication
-------------
If you found a bug and can provide steps to reliably reproduce it, or if you
have a feature request, please
[open an issue](https://github.com/usnistgov/BiometricEvaluation/issues). Other
questions may be addressed to the 
[project maintainers](mailto:beframework@nist.gov).

Pull Requests
-------------
Thanks for your interest in submitting code to Biometric Evaluation Framework.
In order to maintain our project goals, pull requests must:

 * adhere to the existing coding style;
 * use Framework types consistently wherever possible;
 * compile without warning under OS X and RHEL/CentOS 7.x;
 * only make use of POSIX APIs;
 * be in the public domain.

Pull requests may be subject to additional rules as imposed by the National
Institute of Standards and Technology. *Please contact the maintainers*
**before** starting work on or submitting a pull request.

Credits
-------
Biometric Evaluation Framework is primarily maintained by Wayne Salamon and
Gregory Fiumara, featuring code from several NIST contributors. This work has
been sponsored by the National Institute of Standards and Technology, the
Department of Homeland Security, and the Federal Bureau of Investigation.

Citing
------
If you use Biometric Evaluation Framework in the course of your work, please
consider linking back to
[our website](http://www.nist.gov/itl/iad/ig/framework.cfm) or citing
[our manuscript](http://ieeexplore.ieee.org/xpl/articleDetails.jsp?arnumber=7358800):

Fiumara, G.; Salamon, W.; Watson, C, "Towards Repeatable, Reproducible, and
Efficient Biometric Technology Evaluations," in Biometrics: Theory,
Applications and Systems (BTAS), 2015 IEEE 7th International Conference on,
Sept. 8 2015-Sept 11 2015.

### BibTeX
```latex
@INPROCEEDINGS{7358800,
	author={Gregory Fiumara and Wayne Salamon and Craig Watson},
	title={{Towards Repeatable, Reproducible, and Efficient Biometric
	Technology Evaluations}},
	booktitle={Biometrics Theory, Applications and Systems (BTAS), 2015 IEEE
	7th International Conference on}, 
	year={2015},
	pages={1-8},
	doi={10.1109/BTAS.2015.7358800},
	month={Sept}
}
```

License
-------
Biometric Evaluation Framework is released in the public domain. See the 
[LICENSE](https://github.com/usnistgov/BiometricEvaluation/blob/master/LICENSE.md)
for details.


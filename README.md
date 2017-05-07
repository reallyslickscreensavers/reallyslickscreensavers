# Really Slick Screensavers
[http://www.reallyslick.com](http://www.reallyslick.com)

All the savers in this distribution are licensed under the Gnu GPL version 2.
The supporting libraries are licensed under the Gnu LGPL version 2.1.  Any
copying, modifying, or redistribution must follow the terms of these licenses.

Contact information is available on the website.

# Dependencies:

* [OpenAL](www.openal.org) — required for **Skyrocket**.
* [FreeGLUT](http://freeglut.sourceforge.net) — required for **ImplicitDemo**
* [Inno Setup](http://www.jrsoftware.org/isinfo.php) — required to build installer executable.

# Compilation notes:

Currently, all the projects and solutions are using `Visual Studio 2015` (`v140` compiler) to build. All neccessary dependencies are included as submodules in `3rdparty` folder. In order to build `*.scr` files, you can simply open `src\rssavers.sln` solution file and build `Debug` or `Release` configurations. Output files will be placed in `.\bin` directory

In order to build installer your dev-environment must have Inno Setup installed with location of `ìscc.exe` added to your system `%PATH%` environment variable. Open the same `src\rssavers.sln` solution file and buid `Installer` configuration. Resulting installer executable could be found in `installer\output\*.exe` on successfull build.

# Changes:

* **v 0.2.x**
Latest available sources were imported from [original svn repository](https://sourceforge.net/projects/rssavers/). Project files and folders structure were re-arranged a little. Projects and solution files were converted to `Visual Studio 2015` format. Additional dependencies were added as submodules with redistributable packages.

*TODO: Update those parts of code and makefiles?*
There are bits of X Windowing System (X11) code among the rssavers source code. However, the code in this project has never been fully converted to work as *nix screensavers. Hyperspace and Microcosm were actually written in Linux originally and then ported to Windows. This is because gdb and ddd tend to work much better than the Visual Studio debuggers and because Microcosm required much performance profiling with gprof, which cannot be done with the Express Edition of Visual Studio.  Perhaps the paid versions of VS have decent profilers.

# Distribution notes:

*TODO: possible obsolete note, check this later:*
`bin/GPL.txt` and `bin/README.txt` do not appear to maintain their DOS-style
carriage returns when stored in svn.  They must be repaired with unix2dos
before being distributed.

# CAsyCOM

A Component Object Model defines a framework for creating objects and
calling their methods independently of object location. This allows
distributing objects used by one process among multiple executables or
multiple computers. There are numerous COM implementations currently
in use, such as the classic CORBA, Microsoft's DCOM upon which Windows
is built, and, to some degree, DBUS, ubiqutous on Linux due to its use
by systemd.

Casycom is one such COM framework, providing object creation and
lifetime management, fully asynchronous messaging and event loop, and
transparent access to out-of-process objects. It is very lightweight,
a static library compiled to only 20k, and at only 3k lines of code,
it is easy to fully review and understand.

Building requires a c11-supporting compiler, such as gcc 4.6+:

```sh
./configure && make check && make install
```

Read documentation and tutorials in [docs/](docs/index.html)

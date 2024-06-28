JavaScript::V8::XS - Perl XS binding for the
[V8](https://developers.google.com/v8/) Javascript engine.

# Building

In order to build you can just run `perl Makefile.PL` and then do `make`.

We also offer a Docker image to make building easier:

```bash
docker build . -t javascript-xs-builder:latest
docker run --rm --it -v $(pwd):/v8 -w /v8 javascript-xs-builder:latest
perl Makefile.PL
make -j `nproc`
```

# CSV Explorer

I kept on running into problems with opening CSV exports from eg Microsoft Sentinel on my work machine's Excel installation due to the (default) French locale. This resulted in Excel localising it's CSV parser, which rendered each line in a single field.

I needed something quick and dirty to allow me to preview and analyse CSV files. This is my solution for that.

Windows binaries can be found in the releases section, but it has been tested with Linux (Ubuntu 24.04) and macOS.


## Building

Build and cross-build instructions are in [BUILDING.md](BUILDING.md).


## License

This project is licensed under the MIT License. See `LICENSE`.

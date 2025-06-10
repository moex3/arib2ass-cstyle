C program for converting arib subtitles from .ts files into .ass and .srt formats using [libavcodec](https://ffmpeg.org/libavcodec.html) and [libaribcaption](https://github.com/xqq/libaribcaption).

## Installation
You will need to have these dependencies before building:
- libavcodec
- libavformat
- libavutil
- freetype2
- cmake
- make
- pkg-config
- C and C++ compiler

then
```bash
git clone --recurse-submodules https://codeberg.org/moex3/arib2ass-cstyle
cd arib2ass-cstyle
make -j4
./a2ac -h
```
Windows builds are available under the releases tab.

## Usage
### .ass
By default, it will try to match the original arib subtitle format as close as possible.
```bash
ass -o out.ass
```
![](https://ra.thesungod.xyz/CHne91o1.png)

Positioning within in the middle of the bounding box can be turned off with `-no-center-spacing`.
Look at く.
The difference could be more easily seen with the `--debug-boxes` option.
```bash
ass --no-center-spacing -o out.ass
```
![](https://ra.thesungod.xyz/hMQqcTdZ.png)

The width of each character is assumed to be constant. If it is not, it will be padded to fit the width expected by the ARIB format.
This can be turned off with the `--constant-spacing` option, with a spacing value in pixels.
```bash
ass --constant-spacing 4 -o out.ass
```
![](https://ra.thesungod.xyz/cpPpEFI2.png)

As the ruby position is specified in absolute pixels in the ARIB format, the ruby text will not be at its expected place once the character positions change.
Using the `--shift-ruby` option, it will be moved to its correct place.
```bash
ass --constant-spacing 4 --shift-ruby -o out.ass
```
![](https://ra.thesungod.xyz/hz-TESZ7.png)

Some fonts have a smaller real character size than other fonts at the same font size.
One of them is the IPAexGothic font.
The font size is 36 here, with a real character width of 33.
```bash
ass -o out.ass --font fonts/ipaexg.ttf
```
![](https://ra.thesungod.xyz/76VjxO1b.png)

Using the `--fs-adjust` option, the font size will be changed to 39, with a real character width of 36, which is the expected size.
```bash
ass --fs-adjust -o out.ass --font fonts/ipaexg.ttf
```
![](https://ra.thesungod.xyz/P1grPd24.png)

Selecting a specific font from a .ttc font collection file can be done with the `--font-face` option, which expects either a numerical index,
or the text name of the font.
The list of fonts contained in a .ttc file can be listed with the `fc-query` command.
To use the `MS PGothic` font for example, use `ass -o out.ass -f fonts/MSGOTHIC.TTC --font-face 'MS PGothic'`.
Some universal font should be used that most users will have installed, or bundle the font file with the mkv, otherwise the formatting might be weird (mainly the unaligned ruby text).

### .srt
The default is just an srt file without any tags.
```bash
srt -o out.srt
```
![](https://ra.thesungod.xyz/DQ-P9OgV.png)

Given the `--tags` option, additional tags will also be written, like color, bold, italic etc.
Here, the `--furi` option is also enabled, which will try to find the characters meant for the given furigana text, and write it after that.
It should not be used, as it will not be able to find the correct characters most of the time.
```bash
srt -o out.srt --tags --furi
```
![](https://ra.thesungod.xyz/vHosbLfL.png)

### config file
It's possible to save all options into a config file.
The option `--dump-config` will save the currently set options into a file that can be later loaded with `--config-file`.

## Issues
This software is still in early developement, so it might crash or produce incorrect output.
If that happens, feel free to create an issue :)

## Links
- https://github.com/maki-rxrz/Caption2Ass_PCR
- https://codeberg.org/moex3/arib2ass-cstyle
- https://github.com/moex3/arib2ass-cstyle

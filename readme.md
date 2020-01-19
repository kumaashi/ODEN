# ODEN

## About

ODEN is design ideas for the complexity of modern GPU API also neither a special library nor a namespace.

ODEN is a concept dedicated to processing only abstract commands and is not mandatory. This is a so-called command pattern.
Also it makes sense to focus on the impatient and those made with common compatibility commands for C/C++ and Win10.
Modern GPUAPIs are difficult and cumbersome just to do the basics.

However, there isn't much that you need. And since every extension API builds on its basics, ODEN made it because I wanted that abstract layer.
Also DirectX is used for the sample source.

## BASIC Command sets.

ODEN has tiny command sets(CMD). The command *example* is as follows:

```
enum {
	CMD_NOP,c
	CMD_SET_BARRIER,
	CMD_SET_RENDER_TARGET,
	CMD_SET_TEXTURE,
	CMD_SET_VERTEX,
	CMD_SET_INDEX,
	CMD_SET_CONSTANT,
	CMD_SET_SHADER,
	CMD_CLEAR,
	CMD_CLEAR_DEPTH,
	CMD_DRAW_INDEX,
	CMD_MAX,
};
```

ODEN has simple loop as follows,

```
1) Create CMD for sameone(C++/Native, webapi, etc)
2) Import CMD for ODEN func specification.
3) Call *mono* func(aka oden_present_graphics) and performs platform-specific API(or same behavior) processing.
4) Go to 1)
```

All you have to do is implement the command you want to process so that it behaves almost equivalently on the platform.
In the meantime, implementing the command in a meaningful way ensures that it will work on any platform. By processing the command in the oden_present_graphics function, you will be able to concentrate the processing and concentrate on more creative work.


## Graphics API supported by the sample program.

```
DirectX11
DirectX12
Vulkan(WIP)
```
with the VS2017

## Usage

Open oden.sln and choose full build, and run sample code.

## Why the name ODEN?

ODEN is traditional japanese food for the night. ODEN puts various ingredients in one cooking pot.
And sometimes it can be skewered and served more.

It makes sense to handle various components straight and intuitively.
The rest is I like ODEN very much.

That is philosophy.

Why use ODEN? In short, to sketch every time a new API comes out. Ideally, smart class design can be done from the beginning, but there are many patterns that are not. 
Furthermore, if you want to try out the API right now, but create a class design and dependencies from scratch, I felt that it was best to be able to process things flat. This means that as complexity increases, ODEN will stop working ;-).

## Todo

Add variation sample code.

## Sample program LICENSE

MIT License
https://opensource.org/licenses/mit-license.php

## AStyle options

http://astyle.sourceforge.net/
astyle <sources> --indent=tab --style=linux --indent=force-tab --indent-after-parens --indent-col1-comments --pad-header

## Author

gyabo(aka yasai kumaashi)


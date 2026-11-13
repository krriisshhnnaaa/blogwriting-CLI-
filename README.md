# blogWRITER

A small C++17 command-line tool for writing blog posts: write the text in your
editor, optionally generate a cover image with Gemini, then render everything
into a standalone HTML page.

## Requirements

- g++ (C++17) and `make` (or CMake 3.16+)
- libcurl, zlib and [nlohmann/json](https://github.com/nlohmann/json) development headers
  - Debian/Ubuntu: `sudo apt install build-essential libcurl4-openssl-dev zlib1g-dev nlohmann-json3-dev`
- `nano` (or any editor set in `$EDITOR`)
- A Gemini API key, only if you want AI-generated images

## Build

```sh
make            # or: cmake -B build && cmake --build build
```

## Setup

Create a `.env` file in the project root (it is gitignored):

```
GEMINI_API_KEY=your-key-here
```

The `GEMINI_API_KEY` environment variable works too.

## Usage

Run commands from the project root (paths are relative to it).

```sh
./blogwriter                 # interactive mode
./blogwriter write my-post   # or run a single command
```

| Command | What it does |
| --- | --- |
| `list` | Lists all blogs written up until now |
| `write <title>` | Opens `<title>.txt` in `$EDITOR` (default `nano`); a copy is kept in `temp/` |
| `prep <title>` | Sends the post to Gemini and saves a 3:2 image to `assets/` |
| `make <title>` | Renders `blogs/<title>.html` from `template/template.html` |
| `mark <title>` | Parses `<title>.txt` with Gemini for metadata, saves `<title>.md`, and syncs via `save.sh` |
| `publish <title>` | Renders HTML & assets, creates `post.md`, and registers the post in `blog-index.json` |
| `upload <title>` | Pushes changes to GitHub (`git add . ; git commit -m "added <title>" ; git push`) |
| `help`, `exit` | Show commands / quit |

Titles may contain letters, digits, spaces, `-` and `_`.

Typical flow: `write my-post` -> `prep my-post` -> `make my-post`, then open
`blogs/my-post.html` in a browser.

If image generation fails (no key, network or API error), `prep` says so, writes
a clearly labelled placeholder gradient to `assets/`, and `make` still works.

## Project layout

```
src/        main.cpp (CLI), writer.cpp (write/prep/make), gemini.cpp + ai.cpp (Gemini API),
            renderer.cpp (template -> HTML), util.h (title validation, HTML escaping)
template/   template.html and background images
temp/       working copies of post text
assets/     generated images
blogs/      rendered pages
```

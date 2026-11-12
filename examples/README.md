# Examples

Sample scripts showing what rpn can do. Each one also runs unchanged under
GNU dc. From the repo root:

```sh
$ ./rpn examples/mandelbrot.rpn
$ dc examples/mandelbrot.rpn
```

In a release bundle (the `examples/` folder sits next to the binary):

```sh
$ ./rpn examples/mandelbrot.rpn
```

| File | What it shows | Output |
| --- | --- | --- |
| `demo.rpn` | A guided tour of the commands, each result labeled: arithmetic, `^`, `\|`, scale factor `k`, bases, stack ops, strings, macros, registers, loops | labeled lines, e.g. `2 + 3 = 5`, `2 ^ 100 mod 1000 = 376`, `6! computed with a loop = 720` |
| `fibonacci.rpn` | The first N Fibonacci numbers — N is the labeled value at the top of the file | `1 1 2 3 5 8 13 21 34 55 89 144` (with the default N=12) |
| `miller_rabin.rpn` | Miller-Rabin primality test (bases 2..37) — the knob block at the bottom lists the numbers to test, one per line; add your own | `1 0 0 1 0 1 0` for the default 7 numbers (1 = probably prime, 0 = composite) |
| `game_of_life.rpn` | Conway's Game of Life — knobs at the top of the file set the grid size (W, H), generations (T) and the initial pattern as (x, y) cells | 8 grids of `#` (alive) and `.` (dead) on a 10x6 grid, glider by default |
| `mandelbrot.rpn` | ASCII Mandelbrot and Julia sets in fixed-point arithmetic — knobs at the top set scale `k`, grid WxH, max iterations M; each render block sets its window and (for Julia) the constant c | two 40x20 character-ramp renders by default, e.g. `........::::+X*XXXXXXX+XX:.....` |

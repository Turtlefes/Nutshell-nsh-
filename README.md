# Nutshell Unix Shell

![GitHub License](https://img.shields.io/github/license/Turtlefes/Nutshell-nsh-) 
![GitHub Issues](https://img.shields.io/github/issues/Turtlefes/Nutshell-nsh-) 
![Languages](https://img.shields.io/github/languages/top/Turtlefes/Nutshell-nsh-)

**Nutshell** **(or nsh)** - Comes packed with features (and bugs). You get the standard UNIX tools, plus some... interesting additions you won't find anywhere else. Consider the bugs as bonus content.

## Documents
- [Installation](https://github.com/Turtlefes/Nutshell-nsh-/tree/main/docs/install.md)
- [Nutshell builtin](https://github.com/Turtlefes/Nutshell-nsh-/tree/main/docs/builtin.md)
- [Environment variables](https://github.com/Turtlefes/Nutshell-nsh-/tree/main/docs/envs.md)
- [Arithmetic](https://github.com/Turtlefes/Nutshell-nsh-/tree/main/docs/Arithmetic.md)
- [RC File/Control file](https://github.com/Turtlefes/Nutshell-nsh-/tree/main/docs/control_file.md)
- [Changelogs](https://github.com/Turtlefes/Nutshell-nsh-/tree/main/CHANGELOG.md)

## Features

### Run programs and pipelines!
```nsh
$ ls | wc -l
12
$ echo "in here was $(ls | wc -l) file!"
in here was 12 file!

$ echo foo,bar | awk -F "," '{print $2, $1}'
bar foo
```

### With redirections

```
$ cd $HOME && ls | wc -l > output.txt
$ cat output.txt
Downloads
Documents
File.txt
```

### Command substitution
```
$ echo "you have $(ls | wc -l) files inside $(pwd) in $(date)"
you have 3 files inside /user/home in Mon Oct 6 15:39:18 WIB 2025
```

### Run multiple commands (with logical)

```
$ echo foo; echo bar
foo
bar

$ echo foo && echo bar
foo
bar

$ echo foo || echo bar
foo
```

### Math arithmetic in the shell!

```
$ echo $((1 + 2 * 3 - 4))
3
$ echo $(((1 + 2) * (3 - 4) / 8.0))
-0.375
$ echo $((2**31))
2147483648
```
and even a puzzle!
```
$ echo $(?+5=10)
5
$ echo $((?*10=200))
20
```

and other complex math..

## Nutshell also have a easter eggs! (BETA)
You can find it yourself, btw you can crack my shell. (clue)
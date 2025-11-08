# Zmensur File Format

## Overview

The **zmensur format** is an extended version of the traditional mensur format that supports:

- **Variable definitions**
- **Tone hole specifications** (including water keys)
- **Valve branch routing**
- **Comments** throughout the file

This format is used by `calcimp` and `calcspd` for acoustic calculations.

---

## File Structure

### Basic Line Format

Each line in a zmensur file follows this pattern:

```
df,db,length,comment
```

- `df` - Front diameter (mm)
- `db` - Back diameter (mm)
- `length` - Length (mm)
- `comment` - Optional description

### Comments

Lines starting with `%` or text after `%` on any line are treated as comments and ignored.

```
% This entire line is a comment
10,10,100,tube section % This is also a comment
```

### File Header

The first line is treated as a file comment/description:

```
zmensur sample % First line describes the file
```

---

## Special Directives

### Variables

Define variables with `name=value,` syntax:

```
sl1=20,
sl2=15,
% One variable per line
```

### Tube Sections

Standard tube segments use the basic format:

```
10,10,10,cylindrical section
10,12,15,tapered section
```

### Termination

End a mensur (main tube or branch) with `df,0,0,`:

```
15,0,0,     % Open end
0,0,0,      % Closed end
```

---

## Advanced Features

### 1. Addition Segments (`+`)

Insert a sub-mensur defined elsewhere:

```
+add3,1
```

**Format:** `+name,ratio`

- `ratio = 1` - Fully inserted into the main tube
- `ratio = 0` - Not inserted (ignored)
- `0 < ratio < 1` - Creates a branch with partial insertion

### 2. Tone Holes (`-`)

Add a tone hole at this position:

```
-th1,0.8
```

**Format:** `-name,openness`

- `openness = 1` - Fully open hole
- `openness = 0` - Fully closed hole
- `0 < openness < 1` - Partially open hole

The hole geometry is defined in a separate `$name` section.

### 3. Valve Routing (`>` and `<`)

Create valve bypass paths:

```
>valve1,1    % Start of bypass
15,15,20,    % Main path continues
<valve1,1    % End of bypass (rejoins main path)
```

**Format:** `>name,ratio` and `<name,ratio`

- `>` marks the start of a detour
- `<` marks where the detour rejoins
- `ratio = 1` - Valve fully engaged
- `0 < ratio < 1` - Partial valve engagement (half-valve)

### 4. Sub-Mensur Definitions (`$`)

Define reusable tube sections referenced by `+`, `-`, `>`, or `<`:

```
$add3
% Define geometry for "add3"
15,15,200,
15,0,0,

$th1
% Tone hole geometry
4,4,8,
4,0,0,

$valve1
% Valve bypass path
15,15,100,
15,15,20,
15,15,100,
15,0,0,
```

**Important notes:**
- Each sub-mensur must end with a termination line (`df,0,0,`)
- For tone holes: effective diameter = `openness × df`
  - Example: `-th1,0.8` with `df=4` → effective diameter = 3.2mm

---

## Complete Example

```
zmensur sample % File description
% Variable definitions
sl1=20,
sl2=15,

% Main tube body
10,10,10,zmen test % Basic format: df,db,r,comment
10,12,15,taper

+add3,1          % Insert "add3" segment fully

15,15,20,
-th1,0.8         % Tone hole at 80% open

>valve1,1        % Valve bypass starts
15,15,20,
<valve1,1        % Valve bypass ends

15,15,30,
15,0,0,          % Open end termination

% Sub-mensur definitions
$add3
15,15,200,
15,0,0,

$th1
4,4,8,           % With 0.8 ratio → 3.2mm effective diameter
4,0,0,

$valve1
15,15,100,
15,15,20,
15,15,100,
15,0,0,

% End of file
```

---

## Summary Table

| Prefix | Purpose | Format | Notes |
|--------|---------|--------|-------|
| `%` | Comment | `% text` | Ignored by parser |
| (none) | Tube segment | `df,db,length,comment` | Basic geometry |
| `+` | Add segment | `+name,ratio` | Insert sub-mensur |
| `-` | Tone hole | `-name,openness` | 0=closed, 1=open |
| `>` | Valve start | `>name,ratio` | Begin bypass path |
| `<` | Valve end | `<name,ratio` | Rejoin main path |
| `$` | Define sub-mensur | `$name` | Followed by geometry |

---

## Notes

- Each sub-mensur definition's termination condition (open/closed) is specified but may be overridden by the reference ratio
- Valve routing allows modeling complex wind instrument valve systems
- The format is backward-compatible with traditional mensur files
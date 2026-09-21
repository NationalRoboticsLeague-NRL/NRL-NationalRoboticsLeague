# NRL Block Editor — basic TeleOp (canonical AST)

Scope: **basic TeleOp only**. Blocks describe a per-tick `loop()` body — read the
gamepad, drive motors/servos, read IMU heading, print telemetry. No AUTO, no
Action/sequence system. `init()` and `stop()` are **auto-generated** by scanning which
hardware the blocks reference, so students only describe "what happens each tick."

One JSON AST is the **single source of truth**; blocks, the script view, and the C++ are
all projections of it — that's what lets blocks ⟷ text round-trip.

```
                 ┌──────────────────────────┐
   Blockly  ◀───▶│   nrlProgram (JSON AST)  │◀───▶  saved file (DB / .json)
   (block view)  └───┬───────┬───────┬──────┘
        astToText →  │       │       │  ← textToAst (parser, raw fallback)
                     ▼       ▼       ▼
              script view  astToCpp  astToOps
              (readable)   opmodes/  interpreter JSON (Path B, secondary)
```

## Files
- `nrl-ast.mjs` — the core. `astToText`, `textToAst`, `astToCpp`, `astToOps`. Zero deps, ESM.
- `index.html` — the drag-and-drop editor (serve over http, see below).
- `demo.mjs` — `node tools/blocks-editor/demo.mjs` proves round-trip + codegen.

## Opening the editor (must be served, not file://)
Run any one static server from the repo root, then open `http://localhost:8000/`:
```bash
# Windows
python  -m http.server 8000 --directory tools/blocks-editor
# macOS / Linux (python is python3 there)
python3 -m http.server 8000 --directory tools/blocks-editor
# or, Node — identical on every OS:
npx serve -l 8000 tools/blocks-editor
```
`file://` breaks the ES-module import — the page comes up blank. Always use the http URL.

Cross-platform note: the editor (HTML/JS), the AST core, and `demo.mjs` (Node) behave
identically on Windows, macOS, and Linux — no native deps, no OS-specific paths. Only the
launch command differs (`python` vs `python3`, or use `npx serve`). Flashing the robot is
your existing PlatformIO flow and is unchanged (serial ports show as `COM*` on Windows,
`/dev/cu.*` on macOS — `pio` auto-detects both).

## The AST (TypeScript view of the JSON)
```ts
interface Program { nrlProgram: { version:1; name:string; mode:"TELEOP"; body: Stmt[] }}

type Stmt =
  | { kind:"driveTank";  forward:Expr; turn:Expr }          // drive.drive(f,t)  (±1.0)
  | { kind:"setMotor";   side:"left"|"right"; power:Expr }  // motor.setSpeed(±255)
  | { kind:"driveStop" }                                    // drive.stop()
  | { kind:"setServo";   name:string; pos:Expr }            // servo.setPosition(0..180)
  | { kind:"zeroHeading" }                                  // NRLComms::zeroHeading()
  | { kind:"telemetry";  key:string; value:Expr }           // telemetry.addData(key,v)
  | { kind:"if";         cond:Expr; then:Stmt[]; else?:Stmt[] }
  | { kind:"rawStmt";    code:string };                     // ◀ ESCAPE HATCH (gray block)

type Expr =
  | { kind:"num"; value:number } | { kind:"bool"; value:boolean }
  | { kind:"gamepadAxis";   stick:"leftX"|"leftY"|"rightX"|"rightY" }   // gamepad1.leftY()
  | { kind:"gamepadButton"; btn:string; edge:"pressed"|"justPressed"|"justReleased" }
  | { kind:"imuHeading" }                                   // NRLComms::getHeading()
  | { kind:"compare"; op:string; left:Expr; right:Expr }
  | { kind:"math";    op:string; left:Expr; right:Expr }
  | { kind:"logic";   op:"&&"|"||"; left:Expr; right:Expr }
  | { kind:"not";     value:Expr }
  | { kind:"rawExpr"; code:string };                        // ◀ ESCAPE HATCH
```

## Script grammar (what `astToText`/`textToAst` speak)
```
teleop "My TeleOp":
  drive forward stick leftY turn stick rightX
  motor left power 200
  servo claw to 120
  zero heading
  show "heading" = heading
  if button A just pressed:
    servo claw to 0
  else:
    servo claw to 120
  stop driving
```
Expressions: `stick <axis>`, `button <NAME> pressed|just pressed|just released`, `heading`,
numbers, `true/false`, `a > b`, `a and b`, `a or b`, `not a`, parentheses, `+ - * /`.

## Auto-generated init()/stop() (hardware scan)
`collectHardware()` walks the body (recursing if-branches) and:
- declares `leftMotor`/`rightMotor` if any drive/motor block is used;
- declares `TankDrive drive` only if a tank-drive / stop-driving block is used;
- declares one `HexaServo <name>` per unique servo name, on `SERVO_1..4` in first-seen order;
- `init()` calls servo `begin()` **first**, then motor `begin()` (LEDC-timer ordering);
- `stop()` calls `drive.stop()` (or per-motor `stop()`) + `detach()` on each servo.

IMU heading and telemetry need no declaration (framework-managed / NRLOpMode member).

## Round-trip boundary
Known `kind` + no `raw*` descendants → round-trips losslessly (block ⟷ text). Anything the
parser can't structure → `rawStmt`/`rawExpr`: still renders to text, C++, and a gray block,
but can't be split further. Example: `motor left power leftMotor.getCurrentPosition() * 0.5`
parses the statement but stores the argument as `{kind:"rawExpr", code:"..."}`.

## Using a generated opmode
`astToCpp` → save as `RobotFirmware/opmodes/<Name>.cpp` → `pio run -t upload`. The build
compiles `opmodes/**` (`build_src_filter`), so it appears in the TELEOP menu.

## Mapping to real NRL APIs (verified against the codebase)
| AST | generated C++ |
|---|---|
| `driveTank` | `drive.drive(fwd, turn)` (±1.0) |
| `setMotor` | `leftMotor.setSpeed(n)` / `rightMotor.setSpeed(n)` (±255) |
| `driveStop` | `drive.stop()` |
| `setServo` | `servo.setPosition(n)` (auto-declared, begun before motors, detached on stop) |
| `zeroHeading` | `NRLComms::zeroHeading()` |
| `telemetry` | `telemetry.addData("k", v)` |
| `imuHeading` | `NRLComms::getHeading()` |
| `gamepadAxis` | `gamepad1.leftY()` etc. |
| `gamepadButton` | `gamepad1.pressed/justPressed/justReleased(BTN_X)` |

## Deferred (not in this version)
OLED blocks, AUTO mode / Action sequences, the Path B interpreter firmware.

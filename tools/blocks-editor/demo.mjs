// ============================================================
//  demo.mjs — proves the TeleOp core end-to-end.
//  Run:  node tools/blocks-editor/demo.mjs
// ============================================================
import { astToText, textToAst, astToCpp, astToOps } from "./nrl-ast.mjs";

const line = (t) => console.log("\n" + "=".repeat(64) + "\n  " + t + "\n" + "=".repeat(64));

// 1) Canonical AST (what gets saved / what Blockly edits)
const prog = {
  nrlProgram: {
    version: 1, name: "My TeleOp", mode: "TELEOP",
    body: [
      { kind: "driveTank",
        forward: { kind: "gamepadAxis", stick: "leftY" },
        turn:    { kind: "gamepadAxis", stick: "rightX" } },
      { kind: "if",
        cond: { kind: "gamepadButton", btn: "A", edge: "justPressed" },
        then: [ { kind: "setServo", name: "claw", pos: { kind: "num", value: 0 } } ],
        else: [ { kind: "setServo", name: "claw", pos: { kind: "num", value: 120 } } ] },
      { kind: "if",
        cond: { kind: "gamepadButton", btn: "RB", edge: "pressed" },
        then: [ { kind: "setMotor", side: "left", power: { kind: "num", value: 200 } } ] },
      { kind: "telemetry", key: "heading", value: { kind: "imuHeading" } },
    ],
  },
};

line("AST  →  TEXT (script view)");
const text = astToText(prog);
console.log(text);

line("TEXT  →  AST  →  TEXT   (round-trip must be identical)");
const text2 = astToText(textToAst(text));
console.log(text2);
console.log(text === text2
  ? ">>> ROUND-TRIP OK: text is stable through AST <<<"
  : ">>> ROUND-TRIP MISMATCH <<<");

line("ROUND-TRIP BOUNDARY: a hand-edit the grammar can't structure");
const edited = `teleop "My TeleOp":
  drive forward stick leftY turn stick rightX
  motor left power leftMotor.getCurrentPosition() * 0.5`;
const back = textToAst(edited);
console.log("setMotor power became:", JSON.stringify(back.nrlProgram.body[1].power));
console.log("^ kind:'rawExpr' — renders everywhere, but can't split into blocks");

line("AST  →  C++  (drop into RobotFirmware/opmodes/, flash as-is)");
const cpp = astToCpp(prog);
console.log(cpp);

line("AST  →  interpreter JSON (Path B)");
console.log(JSON.stringify(astToOps(prog), null, 2));

import { writeFileSync, mkdirSync } from "node:fs";
mkdirSync(new URL("./out/", import.meta.url), { recursive: true });
writeFileSync(new URL("./out/MyTeleOp.cpp", import.meta.url), cpp);
console.log("\nWrote tools/blocks-editor/out/MyTeleOp.cpp");

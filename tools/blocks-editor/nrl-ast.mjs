// ============================================================
//  nrl-ast.mjs — canonical AST core for the NRL block editor
//  SCOPE: basic TeleOp (direct, per-tick loop() body). No AUTO,
//  no Action/sequence system. init()/stop() are auto-generated.
// ============================================================
//
//  ONE source of truth (the JSON AST). Four pure consumers:
//
//      astToText(prog)   → readable "script" view   (printer)
//      textToAst(src)    → script view → AST         (parser, raw fallback)
//      astToCpp(prog)    → flashable opmodes/*.cpp   (TeleOp codegen)
//      astToOps(prog)    → interpreter JSON          (Path B, secondary)
//
//  ROUND-TRIP BOUNDARY: any text the parser can't structure becomes a
//  { kind:"rawStmt" } / { kind:"rawExpr" } node. It still renders to
//  text, C++, and a gray block — it just can't be decomposed further.
// ============================================================

const num  = (value) => ({ kind: "num", value });
const bool = (value) => ({ kind: "bool", value });
const rawE = (code)  => ({ kind: "rawExpr", code });

const AXES    = ["leftX", "leftY", "rightX", "rightY"];
const BUTTONS = ["X", "A", "B", "Y", "RB", "LB", "LT", "RT",
                 "DPAD_UP", "DPAD_DOWN", "DPAD_LEFT", "DPAD_RIGHT"];

// ============================================================
//  1) EXPRESSION PARSER  (recursive descent, raw fallback)
//     precedence: or < and < not < compare < +/- < * /
// ============================================================
function tokenize(s) {
  // Numeric literal (incl. optional leading "-" for unary minus, e.g. "-5") is
  // tried BEFORE the bare operator class, so a minus immediately followed by a
  // digit tokenizes as a signed number instead of "-" then "5". Doesn't affect
  // normal spaced subtraction ("5 - 3") — the digit-lookahead only matches when
  // there's no space between "-" and the following digit.
  const re = /\s*(>=|<=|==|!=|>|<|\(|\)|"[^"]*"|-?\d+\.?\d*|[-+*/]|[A-Za-z_][A-Za-z0-9_]*)\s*/y;
  const toks = []; let i = 0;
  while (i < s.length) {
    re.lastIndex = i;
    const m = re.exec(s);
    if (!m || m.index !== i) return null;
    toks.push(m[1]); i = re.lastIndex;
  }
  return toks;
}

function parseExprSafe(s) {
  s = s.trim();
  const toks = tokenize(s);
  if (!toks) return rawE(s);
  let p = 0;
  const peek = () => toks[p];
  const next = () => toks[p++];

  function primary() {
    const t = peek();
    if (t === undefined) throw 0;
    if (t === "(") { next(); const e = orE(); if (next() !== ")") throw 0; return e; }
    if (/^-?\d+\.?\d*$/.test(t)) { next(); return num(parseFloat(t)); }
    if (t === "true" || t === "false") { next(); return bool(t === "true"); }
    if (t === "heading") { next(); return { kind: "imuHeading" }; }
    if (t === "stick") { next(); const a = next(); if (!AXES.includes(a)) throw 0;
                         return { kind: "gamepadAxis", stick: a }; }
    if (t === "button") {
      next(); const b = next(); if (!BUTTONS.includes(b)) throw 0;
      let edge;
      if (peek() === "pressed") { next(); edge = "pressed"; }
      else if (peek() === "just") {
        next();
        if (peek() === "pressed") { next(); edge = "justPressed"; }
        else if (peek() === "released") { next(); edge = "justReleased"; }
        else throw 0;
      } else throw 0;
      return { kind: "gamepadButton", btn: b, edge };
    }
    // No statement kind declares a variable, so a bare identifier here can never
    // be a legitimate reference (only a typo of a keyword above) — fall through
    // to the rawExpr fallback below instead of silently emitting an uncompilable
    // C++ reference. (kind:"var" nodes still round-trip correctly if they already
    // exist in a saved AST — see the "var" cases in astToText/astToCpp/astToOps.)
    throw 0;
  }
  const mul = () => { let l = primary();
    while (peek() === "*" || peek() === "/") l = { kind:"math", op:next(), left:l, right:primary() };
    return l; };
  const add = () => { let l = mul();
    while (peek() === "+" || peek() === "-") l = { kind:"math", op:next(), left:l, right:mul() };
    return l; };
  const cmp = () => { let l = add();
    if ([">","<",">=","<=","==","!="].includes(peek())) l = { kind:"compare", op:next(), left:l, right:add() };
    return l; };
  const notE = () => { if (peek() === "not") { next(); return { kind:"not", value:notE() }; } return cmp(); };
  const andE = () => { let l = notE();
    while (peek() === "and") { next(); l = { kind:"logic", op:"&&", left:l, right:notE() }; }
    return l; };
  const orE = () => { let l = andE();
    while (peek() === "or") { next(); l = { kind:"logic", op:"||", left:l, right:andE() }; }
    return l; };
  try { const e = orE(); if (p !== toks.length) return rawE(s); return e; }
  catch { return rawE(s); }
}

// ============================================================
//  2) astToText — AST → readable script
// ============================================================
const PREC = (e) =>
  e.kind === "logic" ? (e.op === "||" ? 1 : 2) :
  e.kind === "not" ? 3 :
  e.kind === "compare" ? 4 :
  e.kind === "math" ? (e.op === "+" || e.op === "-" ? 5 : 6) : 7;

function exprToText(e) {
  const wrap = (child, isRight) => {
    const t = exprToText(child);
    return (PREC(child) < PREC(e) || (PREC(child) === PREC(e) && isRight)) ? `(${t})` : t;
  };
  switch (e.kind) {
    case "num":           return String(e.value);
    case "bool":          return e.value ? "true" : "false";
    case "var":           return e.name;
    case "imuHeading":    return "heading";
    case "gamepadAxis":   return `stick ${e.stick}`;
    case "gamepadButton": return `button ${e.btn} ` +
      (e.edge === "pressed" ? "pressed" : e.edge === "justPressed" ? "just pressed" : "just released");
    case "not":           return `not ${wrap(e.value, false)}`;
    case "logic":         return `${wrap(e.left, false)} ${e.op === "&&" ? "and" : "or"} ${wrap(e.right, true)}`;
    case "compare":
    case "math":          return `${wrap(e.left, false)} ${e.op} ${wrap(e.right, true)}`;
    case "rawExpr":       return e.code;
    default:              return `/*?${e.kind}*/`;
  }
}

const pad = (n) => "  ".repeat(n);

function stmtToText(s, d) {
  const L = (t) => pad(d) + t;
  switch (s.kind) {
    case "driveTank":  return L(`drive forward ${exprToText(s.forward)} turn ${exprToText(s.turn)}`);
    case "setMotor":   return L(`motor ${s.side} power ${exprToText(s.power)}`);
    case "driveStop":  return L(`stop driving`);
    case "setServo":   return L(`servo ${s.name} to ${exprToText(s.pos)}`);
    case "zeroHeading":return L(`zero heading`);
    case "telemetry":  return L(`show "${s.key}" = ${exprToText(s.value)}`);
    case "rawStmt":    return L(s.code);
    case "if": {
      let out = L(`if ${exprToText(s.cond)}:`) + "\n" + stmtsToText(s.then, d + 1);
      if (s.else && s.else.length) out += "\n" + L(`else:`) + "\n" + stmtsToText(s.else, d + 1);
      return out;
    }
    default: return L(`/*?${s.kind}*/`);
  }
}

const stmtsToText = (list, d) =>
  list.length ? list.map(s => stmtToText(s, d)).join("\n") : pad(d) + "pass";

function astToText(prog) {
  const P = prog.nrlProgram;
  return `teleop "${P.name}":\n` + stmtsToText(P.body || [], 1) + "\n";
}

// ============================================================
//  3) textToAst — script → AST
// ============================================================
function lexLines(src) {
  const out = [];
  src.split(/\r?\n/).forEach((raw, i) => {
    if (!raw.trim() || /^\s*#/.test(raw)) return;
    out.push({ indent: raw.match(/^ */)[0].length, text: raw.trim(), line: i + 1 });
  });
  return out;
}

function buildForest(lines) {
  let pos = 0;
  function children(parentIndent) {
    const nodes = [];
    while (pos < lines.length && lines[pos].indent > parentIndent) {
      const cur = lines[pos++];
      nodes.push({ text: cur.text, children: children(cur.indent) });
    }
    return nodes;
  }
  return children(-1);
}

const stripColon = (t) => t.replace(/:\s*$/, "");

function interpretStmts(nodes) {
  const out = [];
  for (let i = 0; i < nodes.length; i++) {
    const n = nodes[i];
    const head = stripColon(n.text);
    let m;
    if ((m = head.match(/^if\s+(.+)$/)) && n.text.endsWith(":")) {
      const node = { kind: "if", cond: parseExprSafe(m[1]), then: interpretStmts(n.children), else: [] };
      if (i + 1 < nodes.length && stripColon(nodes[i + 1].text) === "else")
        node.else = interpretStmts(nodes[++i].children);
      out.push(node);
      continue;
    }
    const s = interpretStmt(n);
    if (s) out.push(s);
  }
  return out;
}

function interpretStmt(n) {
  const t = n.text, head = stripColon(t);
  let m;
  if ((m = head.match(/^drive forward\s+(.+?)\s+turn\s+(.+)$/)))
    return { kind: "driveTank", forward: parseExprSafe(m[1]), turn: parseExprSafe(m[2]) };
  if ((m = head.match(/^motor\s+(left|right)\s+power\s+(.+)$/)))
    return { kind: "setMotor", side: m[1], power: parseExprSafe(m[2]) };
  if (head === "stop driving")  return { kind: "driveStop" };
  if ((m = head.match(/^servo\s+(\w+)\s+to\s+(.+)$/)))
    return { kind: "setServo", name: m[1], pos: parseExprSafe(m[2]) };
  if (head === "zero heading")  return { kind: "zeroHeading" };
  if ((m = head.match(/^show\s+"([^"]*)"\s*=\s*(.+)$/)))
    return { kind: "telemetry", key: m[1], value: parseExprSafe(m[2]) };
  if (head === "pass") return null;
  return { kind: "rawStmt", code: t };            // ◀── escape hatch
}

function textToAst(src) {
  const forest = buildForest(lexLines(src));
  const header = forest[0];
  const m = header && header.text.match(/^teleop\s+"([^"]*)"\s*:$/);
  if (!m) throw new Error('first line must be: teleop "Name":');
  return { nrlProgram: { version: 1, name: m[1], mode: "TELEOP",
           body: interpretStmts(header.children) } };
}

// ============================================================
//  4) astToCpp — AST → flashable opmodes/*.cpp  (TeleOp)
// ============================================================
function exprToCpp(e) {
  switch (e.kind) {
    case "num":           return Number.isInteger(e.value) ? String(e.value) : `${e.value}f`;
    case "bool":          return e.value ? "true" : "false";
    case "var":           return e.name;
    case "imuHeading":    return "NRLComms::getHeading()";
    case "gamepadAxis":   return `gamepad1.${e.stick}()`;
    case "gamepadButton": return `gamepad1.${e.edge}(BTN_${e.btn})`;
    case "not":           return `!(${exprToCpp(e.value)})`;
    case "logic":
    case "compare":
    case "math":          return `(${exprToCpp(e.left)} ${e.op} ${exprToCpp(e.right)})`;
    case "rawExpr":       return e.code;
    default:              return "0";
  }
}

const motorVar = (side) => (side === "left" ? "leftMotor" : "rightMotor");

// gather hardware used anywhere in the body (recurses if-branches)
function collectHardware(stmts, hw = { servos: [], usesDrive: false, usesMotors: false }) {
  for (const s of stmts || []) {
    switch (s.kind) {
      case "driveTank": case "driveStop": hw.usesDrive = true; hw.usesMotors = true; break;
      case "setMotor":  hw.usesMotors = true; break;
      case "setServo":  if (!hw.servos.includes(s.name)) hw.servos.push(s.name); break;
      case "if":        collectHardware(s.then, hw); collectHardware(s.else, hw); break;
    }
  }
  return hw;
}

function stmtToCpp(s, indent) {
  const I = pad(indent);
  switch (s.kind) {
    case "driveTank":  return I + `drive.drive(${exprToCpp(s.forward)}, ${exprToCpp(s.turn)});`;
    case "setMotor":   return I + `${motorVar(s.side)}.setSpeed(${exprToCpp(s.power)});`;
    case "driveStop":  return I + `drive.stop();`;
    case "setServo":   return I + `${s.name}.setPosition(${exprToCpp(s.pos)});`;
    case "zeroHeading":return I + `NRLComms::zeroHeading();`;
    case "telemetry":  return I + `telemetry.addData("${s.key}", ${exprToCpp(s.value)});`;
    case "rawStmt":    return I + `${s.code};`;
    case "if": {
      let out = I + `if (${exprToCpp(s.cond)}) {\n` +
                (s.then || []).map(x => stmtToCpp(x, indent + 1)).join("\n") + `\n` + I + `}`;
      if (s.else && s.else.length)
        out += ` else {\n` + s.else.map(x => stmtToCpp(x, indent + 1)).join("\n") + `\n` + I + `}`;
      return out;
    }
    default: return I + `/*?${s.kind}*/`;
  }
}

function astToCpp(prog) {
  const P = prog.nrlProgram;
  const cls = "Blocks" + P.name.replace(/[^A-Za-z0-9]/g, "");
  const hw = collectHardware(P.body || []);

  // ---- file-scope hardware declarations ----
  const decls = [];
  if (hw.usesMotors) {
    decls.push(`static HexaDCMotor leftMotor {{ .dirPin = MOTOR_L_DIR, .pwmPin = MOTOR_L_PWM }};`);
    decls.push(`static HexaDCMotor rightMotor{{ .dirPin = MOTOR_R_DIR, .pwmPin = MOTOR_R_PWM, .flipped = true }};`);
  }
  // settleMs = 0 → servo holds its commanded angle instead of drooping after
  // ~300ms idle (matches the tuned StudentTeleOp1 setup; safe with BEC + caps).
  hw.servos.forEach((n, i) =>
    decls.push(`static HexaServo   ${n}{{ .signalPin = SERVO_${i + 1}, .startAngle = 90.0f, .settleMs = 0 }};`));
  if (hw.usesDrive) decls.push(`static TankDrive    drive(leftMotor, rightMotor);`);

  // ---- init(): servos begin FIRST, then motors ----
  const init = [];
  hw.servos.forEach(n => init.push(`        ${n}.begin();`));
  if (hw.usesMotors) { init.push(`        leftMotor.begin();`); init.push(`        rightMotor.begin();`); }

  // ---- stop(): drive/motors off, servos released ----
  const stop = [];
  if (hw.usesDrive)        stop.push(`        drive.stop();`);
  else if (hw.usesMotors)  { stop.push(`        leftMotor.stop();`); stop.push(`        rightMotor.stop();`); }
  hw.servos.forEach(n => stop.push(`        ${n}.detach();`));

  const loop = (P.body || []).map(s => stmtToCpp(s, 4)).join("\n");

  return `// ============================================================
//  ${P.name} — AUTO-GENERATED from blocks (TeleOp). Do not hand-edit.
//  (Editing here is the one-way "eject to text" door — see README.)
// ============================================================
#include "NRL.h"

${decls.join("\n")}

class ${cls} : public NRLOpMode {
public:
    void init() override {
${init.join("\n")}
    }

    void loop() override {
${loop}
    }

    void stop() override {
${stop.join("\n")}
    }
};

REGISTER_OPMODE(${cls}, "${P.name}", TELEOP);
`;
}

// ============================================================
//  5) astToOps — AST → interpreter JSON  (Path B, secondary)
// ============================================================
function exprToOp(e) {
  switch (e.kind) {
    case "num": case "bool": return e.value;
    case "imuHeading":       return { fn: "heading" };
    case "gamepadAxis":      return { fn: "axis", a: e.stick };
    case "gamepadButton":    return { fn: "button", b: e.btn, edge: e.edge };
    case "var":              return { fn: "var", name: e.name };
    case "not":              return { fn: "not", v: exprToOp(e.value) };
    case "logic": case "compare": case "math":
      return { fn: e.kind, op: e.op, l: exprToOp(e.left), r: exprToOp(e.right) };
    case "rawExpr":          return { fn: "raw", code: e.code };
    default:                 return 0;
  }
}
function stmtToOp(s) {
  switch (s.kind) {
    case "driveTank":  return { op: "drive", forward: exprToOp(s.forward), turn: exprToOp(s.turn) };
    case "setMotor":   return { op: "motor", side: s.side, power: exprToOp(s.power) };
    case "driveStop":  return { op: "stop" };
    case "setServo":   return { op: "servo", name: s.name, pos: exprToOp(s.pos) };
    case "zeroHeading":return { op: "zeroHeading" };
    case "telemetry":  return { op: "telemetry", key: s.key, value: exprToOp(s.value) };
    case "if":         return { op: "if", cond: exprToOp(s.cond),
                                then: (s.then || []).map(stmtToOp), else: (s.else || []).map(stmtToOp) };
    default:           return { op: "raw", code: s.code || "" };
  }
}
function astToOps(prog) {
  const P = prog.nrlProgram;
  return { mode: "TELEOP", name: P.name, program: (P.body || []).map(stmtToOp) };
}

export { astToText, textToAst, astToCpp, astToOps, parseExprSafe };

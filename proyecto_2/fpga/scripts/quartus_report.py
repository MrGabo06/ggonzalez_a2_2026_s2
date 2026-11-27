#!/usr/bin/env python3
"""Resources and Fmax of the VPROC designs, read from the Quartus reports.

Usage (from proyecto_2, after compiling the projects in Quartus):
    python3 fpga/scripts/quartus_report.py

Reads output_files/<revision>.{fit.summary,sta.summary,sta.rpt,fit.rpt} of fpga/board and
fpga/jtag, which are not committed, and writes the committed summaries:
    results/quartus/recursos.csv             one row per revision
    results/quartus/recursos_por_modulo.csv  resources per instance
    results/quartus/recursos.md              the same as tables, with the derived figures

A revision whose reports are missing is skipped. Fmax is the worst case of the slow models
(85 C and 0 C) for CLOCK_50, which is the 50 MHz design clock.
"""
import csv
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REVISIONS = [
    ("vproc_de1soc", "board", "Prueba autónoma en la tarjeta (sin JTAG)"),
    ("vproc_jtag", "jtag", "JTAG, backend fpga (propuesta 1)"),
    ("vproc_jtag_stp", "jtag", "JTAG con SignalTap"),
]
DESIGN_CLOCK = "CLOCK_50"
DESIGN_MHZ = 50.0


def read(path):
    return path.read_text(errors="replace") if path.exists() else None


def used_of_total(text, label):
    m = re.search(rf"{re.escape(label)}\s*:\s*([\d,]+)\s*/\s*([\d,]+)\s*\(\s*([<\d.]+)\s*%\s*\)", text)
    return (int(m.group(1).replace(",", "")), int(m.group(2).replace(",", "")), m.group(3)) if m else None


def parse_fit_summary(text):
    registers = re.search(r"Total registers\s*:\s*([\d,]+)", text)
    pins = used_of_total(text, "Total pins")
    return {
        "fitter": re.search(r"Fitter Status\s*:\s*(.+)", text).group(1).strip(),
        "quartus": re.search(r"Quartus Prime Version\s*:\s*(.+)", text).group(1).strip(),
        "device": re.search(r"Device\s*:\s*(\S+)", text).group(1),
        "alms": used_of_total(text, "Logic utilization (in ALMs)"),
        "registers": int(registers.group(1).replace(",", "")),
        "pins": pins[0],
        "mem_bits": used_of_total(text, "Total block memory bits"),
        "ram_blocks": used_of_total(text, "Total RAM Blocks"),
        "dsp": used_of_total(text, "Total DSP Blocks"),
    }


def parse_sta_summary(text):
    """Worst setup and hold slack for the design clock over every timing model."""
    slacks = {"setup": [], "hold": []}
    for block in text.split("Type  :")[1:]:
        kind = re.match(rf"\s*.*Model (Setup|Hold) '{DESIGN_CLOCK}'", block)
        slack = re.search(r"Slack\s*:\s*(-?[\d.]+)", block)
        if kind and slack:
            slacks[kind.group(1).lower()].append(float(slack.group(1)))
    return {k: min(v) if v else None for k, v in slacks.items()}


def parse_fmax(text):
    """{model: Fmax MHz of the design clock} from the 'Model Fmax Summary' tables."""
    result = {}
    for model in re.finditer(r";\s*(Slow \S+ \S+) Model Fmax Summary", text):
        table = text[model.end():model.end() + 700]
        row = re.search(rf";\s*([\d.]+) MHz\s*;\s*[\d.]+ MHz\s*;\s*{DESIGN_CLOCK}\s*;", table)
        if row:
            result[model.group(1)] = float(row.group(1))
    return result


def first_number(cell):
    m = re.match(r"\s*([\d.]+)", cell)
    return float(m.group(1)) if m else 0.0


def parse_entities(text):
    """Rows of 'Fitter Resource Utilization by Entity': one dict per instance."""
    start = text.find("; Fitter Resource Utilization by Entity")
    rows = []
    for line in text[start:].splitlines()[3:]:
        if not re.match(r";\s*\|", line):
            if rows:
                break
            continue
        cells = [c.strip() for c in line.strip().strip(";").split(";")]
        if len(cells) < 15:
            continue
        path = cells[14]
        rows.append({"path": path, "depth": path.count("|"), "name": cells[0].strip("|"),
                     "alms": first_number(cells[1]), "registers": int(first_number(cells[7])),
                     "mem_bits": int(first_number(cells[9])), "m10k": int(first_number(cells[10])),
                     "dsp": int(first_number(cells[11]))})
    return rows


def collect():
    designs, modules = [], []
    for revision, folder, description in REVISIONS:
        out = ROOT / "fpga" / folder / "output_files"
        fit, sta, sta_rpt, fit_rpt = (read(out / f"{revision}.{ext}") for ext in
                                      ("fit.summary", "sta.summary", "sta.rpt", "fit.rpt"))
        if not (fit and sta and sta_rpt and fit_rpt):
            print(f"omitido {revision}: faltan reportes en {out}")
            continue
        info = parse_fit_summary(fit)
        slack = parse_sta_summary(sta)
        fmax = parse_fmax(sta_rpt)
        designs.append({"revision": revision, "description": description, **info, **slack,
                        "fmax_worst": min(fmax.values()) if fmax else None, "fmax_models": fmax})
        for row in parse_entities(fit_rpt):
            modules.append({"revision": revision, **row})
    return designs, modules


def pct(used, total):
    return f"{used / total * 100:.1f} %"


def write_csvs(designs, modules, outdir):
    fields = ["revision", "description", "device", "quartus", "fitter", "alms", "alms_total", "alms_pct",
              "registers", "mem_bits", "mem_bits_total", "ram_blocks", "ram_blocks_total", "dsp", "dsp_total",
              "pins", "fmax_clock50_mhz", "fmax_margin_vs_50mhz", "setup_slack_ns", "hold_slack_ns"]
    with open(outdir / "recursos.csv", "w", newline="") as f:
        writer = csv.DictWriter(f, fields)
        writer.writeheader()
        for d in designs:
            writer.writerow({
                "revision": d["revision"], "description": d["description"], "device": d["device"],
                "quartus": d["quartus"], "fitter": d["fitter"], "alms": d["alms"][0], "alms_total": d["alms"][1],
                "alms_pct": round(d["alms"][0] / d["alms"][1] * 100, 1),
                "registers": d["registers"], "mem_bits": d["mem_bits"][0], "mem_bits_total": d["mem_bits"][1],
                "ram_blocks": d["ram_blocks"][0], "ram_blocks_total": d["ram_blocks"][1],
                "dsp": d["dsp"][0], "dsp_total": d["dsp"][1], "pins": d["pins"],
                "fmax_clock50_mhz": d["fmax_worst"],
                "fmax_margin_vs_50mhz": round(d["fmax_worst"] / DESIGN_MHZ, 3) if d["fmax_worst"] else "",
                "setup_slack_ns": d["setup"], "hold_slack_ns": d["hold"]})
    with open(outdir / "recursos_por_modulo.csv", "w", newline="") as f:
        writer = csv.DictWriter(f, ["revision", "path", "alms", "registers", "mem_bits", "m10k", "dsp"],
                                extrasaction="ignore")
        writer.writeheader()
        writer.writerows(m for m in modules if m["depth"] <= 3)


def design_table(designs):
    heads = [d["revision"] for d in designs]
    rows = [["Descripción"] + [d["description"] for d in designs],
            ["ALMs"] + [f"{d['alms'][0]:,} / {d['alms'][1]:,} ({d['alms'][2]} %)" for d in designs],
            ["Registros"] + [f"{d['registers']:,}" for d in designs],
            ["Bits de memoria"] + [f"{d['mem_bits'][0]:,} ({d['mem_bits'][2]} %)" for d in designs],
            ["Bloques M10K"] + [f"{d['ram_blocks'][0]} / {d['ram_blocks'][1]} ({d['ram_blocks'][2]} %)" for d in designs],
            ["Bloques DSP"] + [f"{d['dsp'][0]} / {d['dsp'][1]} ({d['dsp'][2]} %)" for d in designs],
            ["Pines"] + [str(d["pins"]) for d in designs],
            [f"Fmax de {DESIGN_CLOCK} (peor modelo)"] + [f"{d['fmax_worst']:.2f} MHz" for d in designs],
            ["Fmax / 50 MHz"] + [f"{d['fmax_worst'] / DESIGN_MHZ:.2f}x" for d in designs],
            ["Slack de setup (peor)"] + [f"{d['setup']:+.3f} ns" for d in designs],
            ["Slack de hold (peor)"] + [f"{d['hold']:+.3f} ns" for d in designs],
            ["Compilado"] + [d["fitter"].split(" - ")[-1] for d in designs]]
    lines = ["| | " + " | ".join(heads) + " |", "|---|" + "---|" * len(heads)]
    lines += ["| " + " | ".join(r) + " |" for r in rows]
    return lines


def module_table(modules, revision, total_alms):
    top = [m for m in modules if m["revision"] == revision]
    wanted = [m for m in top if m["depth"] <= 3 and not m["name"].startswith(("seg7", "alt_sld", "sld_"))]
    lines = [f"### Desglose de `{revision}`", "",
             "| Instancia | ALMs | % de los ALMs | Registros | Bits de memoria | M10K | DSP |", "|---|---|---|---|---|---|---|"]
    for m in wanted:
        indent = "&nbsp;&nbsp;" * (m["depth"] - 2) if m["depth"] > 2 else ""
        lines.append(f"| {indent}`{m['name']}` | {m['alms']:,.0f} | {pct(m['alms'], total_alms)} | "
                     f"{m['registers']:,} | {m['mem_bits']:,} | {m['m10k']} | {m['dsp']} |")
        if m["name"].startswith("vproc_core"):
            children = [c for c in top if c["depth"] == 3 and c["path"].startswith(m["path"] + "|")]
            own_alms = m["alms"] - sum(c["alms"] for c in children)
            own_regs = m["registers"] - sum(c["registers"] for c in children)
            lines.append(f"| &nbsp;&nbsp;lógica propia de `vproc_core` (sin submódulos) | {own_alms:,.0f} | "
                         f"{pct(own_alms, total_alms)} | {own_regs:,} | 0 | 0 | 0 |")
    seg = sum(m["alms"] for m in top if m["name"].startswith("seg7") and m["depth"] == 2)
    sld = sum(m["alms"] for m in top if m["depth"] == 2 and m["name"].startswith(("sld_", "auto_signaltap")))
    lines.append(f"| `seg7` (6 displays) y `sld_*` (JTAG, SignalTap) | {seg:,.0f} y {sld:,.0f} | | | | | |")
    return lines + [""]


def derived(designs, modules):
    by = {d["revision"]: d for d in designs}
    lines = ["## Lo que se puede leer", ""]
    if "vproc_jtag" in by:
        d = by["vproc_jtag"]
        core = next((m for m in modules if m["revision"] == "vproc_jtag" and m["name"].startswith("vproc_core")), None)
        bridge = next((m for m in modules if m["revision"] == "vproc_jtag" and m["name"].startswith("vjtag_bridge")), None)
        if core and bridge:
            lines.append(f"- **El procesador cabe con holgura.** `vproc_core` usa {core['alms']:,.0f} ALMs "
                         f"({pct(core['alms'], d['alms'][1])} del chip), {core['registers']:,} registros, {core['m10k']} bloques M10K y "
                         f"{core['dsp']} DSP. El puente JTAG agrega {bridge['alms']:,.0f} ALMs y {bridge['registers']:,} registros, "
                         "más que el procesador en registros, probablemente por los registros de 1064 bits de cada scan JTAG.")
        lines.append(f"- **Margen de reloj.** Fmax de {d['fmax_worst']:.1f} MHz contra los 50 MHz del diseño: "
                     f"{(d['fmax_worst'] / DESIGN_MHZ - 1) * 100:.0f} % de margen. Una propuesta con pipeline y forwarding "
                     "alarga la ruta crítica, y ese margen es lo que se gastaría.")
    if "vproc_jtag" in by and "vproc_jtag_stp" in by:
        a, b = by["vproc_jtag"], by["vproc_jtag_stp"]
        lines.append(f"- **Costo de SignalTap:** {b['alms'][0] - a['alms'][0]:+,} ALMs, {b['registers'] - a['registers']:+,} registros, "
                     f"{b['ram_blocks'][0] - a['ram_blocks'][0]:+} bloques M10K y {b['mem_bits'][0] - a['mem_bits'][0]:+,} bits de memoria "
                     f"(32K muestras de 24 bits). Fmax pasa de {a['fmax_worst']:.1f} a {b['fmax_worst']:.1f} MHz.")
    if "vproc_de1soc" in by and "vproc_jtag" in by:
        lines.append(f"- **Diseño de la tarjeta frente al de JTAG:** {by['vproc_de1soc']['alms'][0]:,} contra {by['vproc_jtag']['alms'][0]:,} ALMs. "
                     "La diferencia es el puente JTAG y la memoria de datos de 64 KB (la del diseño de la tarjeta es de 16 KB y trae el programa, "
                     "la imagen y la salida esperada en ROM).")
    lines.append("- **Los DSP** son los multiplicadores 18×18: 8 de la unidad vectorial (uno por carril) y 2 de la ALU escalar.")
    return lines + [""]


def main():
    designs, modules = collect()
    if not designs:
        sys.exit("No hay reportes de Quartus. Compile los proyectos de fpga/board y fpga/jtag")
    outdir = ROOT / "results" / "quartus"
    outdir.mkdir(parents=True, exist_ok=True)
    write_csvs(designs, modules, outdir)
    lines = ["# Recursos y Fmax del VPROC en la DE1-SoC", "",
             f"Dispositivo {designs[0]['device']} (Cyclone V), {designs[0]['quartus']}. Salen de los reportes del Fitter y del "
             "Timing Analyzer. Fmax es el peor caso de los modelos lentos (85 C y 0 C) para el reloj de 50 MHz.", "",
             "## Por diseño", ""] + design_table(designs) + [""]
    lines += ["## Por módulo", "", "Instancias hasta el nivel de los submódulos del procesador. "
              "Los ALMs incluyen a los hijos.", ""]
    for d in designs:
        lines += module_table(modules, d["revision"], d["alms"][0])
    lines += derived(designs, modules)
    (outdir / "recursos.md").write_text("\n".join(lines), encoding="utf-8")
    print("\n".join(lines))
    print(f"Escrito en {outdir}/recursos.md, recursos.csv y recursos_por_modulo.csv")


if __name__ == "__main__":
    main()

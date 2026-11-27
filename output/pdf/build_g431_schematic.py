from pathlib import Path
from html import escape
import csv, math
from reportlab.pdfgen import canvas

ROOT = Path(__file__).resolve().parent
ROOT.mkdir(parents=True, exist_ok=True)
W,H=1120,790
c=canvas.Canvas(str(ROOT/'SmartDrill_G431_schematic.pdf'), pagesize=(W,H))
c.setTitle('SmartDrill G431 - 230 VAC phase controller - engineering schematic Rev A')
svg=[]; page=0
def line(x1,y1,x2,y2,color='#23384d',dash=False):
    c.setStrokeColor(color); c.setLineWidth(1.3); c.setDash(5,4) if dash else c.setDash()
    c.line(x1,H-y1,x2,H-y2)
    svg.append(f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{color}" stroke-width="1.3"'+(' stroke-dasharray="5 4"' if dash else '')+'/>')
def text(x,y,s,size=12,color='#23384d',bold=False):
    c.setFillColor(color); c.setFont('Helvetica-Bold' if bold else 'Helvetica',size); c.drawString(x,H-y,s)
    svg.append(f'<text x="{x}" y="{y}" fill="{color}" font-family="Arial,sans-serif" font-size="{size}" font-weight="{700 if bold else 400}">{escape(s)}</text>')
def box(x,y,w,h,label='',sub='',color='#23384d'):
    c.setStrokeColor(color);c.setLineWidth(1.3);c.setDash();c.rect(x,H-y-h,w,h)
    svg.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="none" stroke="{color}" stroke-width="1.3"/>')
    if label:text(x+10,y+23,label,12,color,True)
    if sub:text(x+10,y+43,sub,10,color)
def dot(x,y):
    c.setFillColor('#23384d');c.circle(x,H-y,2.5,fill=1,stroke=0)
    svg.append(f'<circle cx="{x}" cy="{y}" r="2.5" fill="#23384d"/>')
def resistor(x,y,w,label,sub=''):
    line(x,y,x+12,y);box(x+12,y-8,w-24,16);line(x+w-12,y,x+w,y)
    text(x+5,y-17,label,11)
    if sub:text(x+5,y+30,sub,10)
def notes(x,y,items,size=12,spacing=23):
    for i,s in enumerate(items):text(x,y+i*spacing,s,size)
def start(title,subtitle):
    global svg,page
    svg=[];page+=1
    text(35,40,'SMARTDRILL / STM32G431CBU6',20,bold=True)
    text(35,65,title,16,bold=True);text(35,88,subtitle,11)
    line(35,104,1085,104)
def finish():
    line(35,745,1085,745)
    text(35,769,'Rev A | 02 Oct 2026 | Engineering design: unbuilt, untested; release checks required before mains use.',10)
    text(1020,769,f'{page} / 5',10)
    (ROOT/f'SmartDrill_G431_sheet_{page}.svg').write_text(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}"><rect width="100%" height="100%" fill="white"/>'+''.join(svg)+'</svg>',encoding='utf-8')
    c.showPage()

start('1. Mains power and triac output','230 VAC / 50 Hz brushed universal motor; existing speed controller bypassed only after circuit verification.')
text(40,140,'MAINS DOMAIN - no connection to MCU ground',13,'#b42318',True)
text(40,191,'J1.L',12,bold=True);line(85,187,130,187)
box(130,164,120,46,'F1 motor fuse','Rating: release check')
line(250,187,300,187);box(300,158,145,60,'K0 pole 1','Hardware motor cutoff')
line(445,187,550,187);text(465,177,'L_SW',11);dot(485,187)
box(550,157,175,60,'M1 drill motor','230 VAC, 1400 W label')
line(725,187,800,187);text(740,175,'MOTOR_A2',11);dot(775,187)
box(800,155,160,65,'Q1 BTA24-800CWRG','1=A1, 2=A2, 3=G')
text(804,232,'A2 left / A1 right / G below',10)
line(960,187,1045,187);line(1045,187,1045,470);text(1000,178,'N_SW',11)
text(40,474,'J1.N',12,bold=True);line(85,470,300,470)
box(300,442,145,56,'K0 pole 2','Same mechanical switch')
line(445,470,1045,470);dot(1045,470)
line(775,187,775,280);resistor(775,280,105,'R1 200R');resistor(880,280,105,'R2 200R')
line(985,280,1025,280);line(1025,280,1025,350)
box(815,325,160,65,'U2 MOC3052M','Output terminals: 6, 4')
line(975,350,1025,350);text(980,341,'6',10)
line(815,350,765,350);text(800,341,'4',10)
line(765,350,765,255);line(765,255,880,255);line(880,255,880,220);text(883,250,'GATE',11)
line(765,350,765,405);resistor(765,405,165,'');text(780,435,'R3 330R / 0.5 W',10);line(930,405,1045,405);dot(1045,405)
text(790,303,'R1/R2: flameproof, pulse-rated, 1 W each',10)
line(775,187,775,125);line(775,125,825,125);line(885,125,930,125);line(939,125,1045,125);line(1045,125,1045,187)
text(790,118,'R4 100R/2 W + C1 47 nF X2/305 VAC',10)
box(825,117,60,16);line(930,116,930,134);line(939,116,939,134)
text(790,144,'Series RC snubber across A2-A1 (starting values)',10)
line(485,187,485,335);box(465,335,40,60);text(390,420,'RV1 275 VAC MOV',10);line(485,395,485,470);dot(485,470)
box(40,530,1045,175,'System branches and protective earth')
notes(55,581,[
 'J1.PE -> original drill chassis / exposed metal: retain original protective-earth connection.',
 'Magnetic base -> retain original supply, switch and interlocks; never route through Q1 or K0 motor cutoff.',
 'PS1 -> separate certified isolated 230 VAC-to-5 VDC / 1 A supply; follow its input fuse and PE requirements.',
 'K0 denotes a mechanically linked, motor-rated two-pole cutoff. Software STOP does not provide isolation.',
 'F1 rating and K0 motor utilization rating require motor nameplate/current/inrush data; not fixed at 10 A.',
 'RV1 across L_SW-N_SW downstream of F1. Preserve original overload/thermal protection and mains EMI filter.'
 ],11,21)
finish()

start('2. Isolated zero-cross detector and gate driver','Replaces the original 6N137 + bridge + HEF40106 circuit. Net names connect sheets; do not join domains.')
line(550,115,550,700,'#b42318',True)
text(55,139,'MAINS SIDE',13,'#b42318',True);text(600,139,'ISOLATED LOW-VOLTAGE SIDE',13,bold=True)
text(50,208,'L_SW');line(85,204,100,204);resistor(100,204,140,'R5 15k / 2 W');line(240,204,315,204)
box(315,175,150,115,'U1 H11AA1');text(325,241,'Antiparallel LEDs',10)
text(320,218,'1 input',10);text(320,268,'2 input',10)
line(315,265,240,265);resistor(100,265,140,'R6 15k / 2 W');line(85,265,100,265);text(50,269,'N_SW')
text(55,323,'R5/R6: flameproof, mains voltage-rated.',11)
text(55,345,'Each dissipates ~0.88 W at 230 V, ~1.07 W at 253 V.',11)
box(625,175,155,115,'U1 output','5 collector / 4 emitter')
text(633,267,'6 base: leave open',10);text(633,284,'3: NC',10)
line(780,204,905,204);dot(820,204);text(840,193,'ZERO_CROSS');line(905,204,1020,204);text(955,229,'PA0 / EXTI0',11)
line(820,204,820,150);text(835,150,'3V3',10);box(812,153,16,36);text(840,176,'R7 10k',10)
line(780,265,850,265);text(856,269,'GND_LV',11)
text(600,325,'ZC is HIGH in a window around each voltage zero.',11)
text(600,347,'Capture BOTH edges; the pulse center estimates zero.',11)
text(600,369,'Single-edge timing requires measured offset compensation.',11)
line(35,395,1085,395)
text(600,431,'+5V_ISO');line(650,440,650,473);resistor(650,473,140,'R8 180R / 0.5 W');line(790,473,815,473)
box(815,447,180,65,'U2 LED side','1 anode / 2 cathode')
text(800,465,'1',10);text(800,508,'2',10)
line(815,495,770,495);line(770,495,770,570)
box(720,570,110,65,'Q2 2N3904','Use B/C/E names')
text(779,565,'C',10);text(701,601,'B',10);text(779,651,'E',10)
line(770,635,770,685);text(780,688,'GND_LV')
text(580,549,'PA5 / FIRE');line(580,555,580,595);resistor(580,595,110,'R9 1k');line(690,595,720,595)
dot(690,595);line(690,595,690,650);box(682,650,16,30);text(575,671,'R10 10k B-E',10);line(690,680,690,685);line(690,685,770,685)
text(850,565,'PA5 HIGH = fire',12,bold=True)
text(850,589,'PA5 LOW = LED off',12)
text(850,613,'Reset pulldown keeps off.',11)
text(850,637,'LED current ~18 mA.',11)
notes(45,452,[
 'U2 output pins 4/6 connect ONLY to sheet 1.',
 'U2 pins 3 and 5 are not connected.',
 'MOC3052: RANDOM-PHASE / 600 V.',
 'Do not replace with zero-cross MOC306x.',
 'R1+R2=400R limits 253 VAC peak to <0.9 A.',
 'Use 200-500 us LED pulses initially;',
 'confirm motor current latching/commutation.',
 'C2 100 nF + C3 10 uF: +5V_ISO to GND_LV,',
 'close to Q2 / U2 LED supply.',
 'Signal polarity differs from old F407 firmware.'
 ],11,24)
finish()

start('3. G431, speed sensor, speed knob and controls','All wiring on this sheet is isolated low-voltage wiring. GND_LV is common; no direct mains connection.')
box(40,130,390,180,'PS1 isolated 5 VDC / 1 A + existing G431 board')
notes(55,180,[
 '+5V_ISO -> board USB VBUS using a power-only cable.',
 'GND_LV -> board USB ground and GND header.',
 'Board 3V3 -> logic rail (use existing regulator).',
 'Do not feed +5V to the 3V3 pin.',
 'No simultaneous PC USB supply without power OR-ing.',
 'C4: 100 nF; C5: 47 uF across local +5V/GND.',
 'SWD: PA13=DIO, PA14=CLK, GND, NRST, 3V3 reference.'
 ],11,19)
box(470,130,610,180,'Hall module -> 3.3 V logic buffer')
notes(485,176,[
 'J3: VCC=+5V_ISO, GND=GND_LV, OUT=HALL_5V.',
 'HALL_5V -> R11 1k -> U3.A; R12 10k: HALL_5V to +5V.',
 'C6 100 pF: U3.A to GND (local interference filter).',
 'U3 SN74LVC1G17DBVR: pin 1 NC, 2 A, 3 GND, 4 Y, 5 VCC.',
 'U3.VCC=3V3; U3.Y -> PA3 / EXTI3; C7 100 nF at U3 supply.',
 'Use ONLY this 5 V-tolerant LVC part, not 74HC at 3.3 V.',
 'C8 100 nF + C9 10 uF at sensor connector.'
 ],11,19)
text(45,346,'Speed knob RV2: 10k linear',13,bold=True)
text(45,377,'3V3');line(95,373,125,373);box(125,355,105,38,'RV2');line(230,373,270,373);text(275,377,'GND_LV')
line(175,393,175,435);resistor(175,435,110,'R13 1k');line(285,435,410,435);text(330,423,'PA1 / ADC1_IN2',11)
dot(310,435);line(310,435,310,460);text(320,463,'C10 100 nF to GND',11)
text(45,489,'Knob voltage range: 0..3V3. Calibrate allowed spindle RPM range.',11)
box(470,335,610,166,'Buttons (software commands, not emergency isolation)')
notes(485,380,[
 'SW1 START: PB0 -> normally-open contact -> GND_LV.',
 'SW2 STOP:  PB1 -> normally-open contact -> GND_LV.',
 'R14 / R15: 10k from PB0 / PB1 to 3V3 respectively.',
 'C11 / C12: 100 nF from PB0 / PB1 to GND respectively.',
 'Buttons are active LOW. Debounce in firmware.',
 'K0 on sheet 1 remains the independent hardware motor cutoff.'
 ],11,20)
box(40,525,1040,183,'Pin assignment and firmware requirements')
notes(55,570,[
 'PA0: zero-cross EXTI both edges. PA3: Hall EXTI selected edge. PA1: ADC speed request.',
 'PA5: GPIO output LOW at reset / initialization; HIGH-only timed fire pulses. PB0/PB1: button inputs.',
 'PA15: I2C1_SCL; PB7: I2C1_SDA (AF4, open drain). PA13/PA14 remain SWD.',
 'TIM3: internal 1 MHz scheduling timer, no external channel needed. IWDG enabled.',
 'Do not reuse PC6 or PB8 until their board connections/leakage are resolved.',
 'Hall magnet must be mechanically retained. RPM = 60*f/pulses_per_revolution.',
 'A motor-shaft sensor needs the gearbox ratio to compute spindle RPM; spindle-mounted sensor does not.'
 ],11,20)
finish()

start('4. LCD1602 I2C interface and connection schedule','Existing 5 V LCD + PCF8574 backpack retained. Use a bidirectional level translator; never pull MCU I2C to 5 V.')
def i2c_channel(y,name,pin,ref):
    text(45,y+4,f'{pin} / {name}_LV');line(180,y,400,y)
    box(400,y-25,220,55,f'{ref} BSS138','S=LV / D=HV / G=3V3')
    line(620,y,880,y);text(890,y+4,f'J4.{name} / {name}_HV')
    dot(250,y);line(250,y,250,y-58);box(242,y-88,16,30);text(180,y-103,'3V3');text(265,y-68,'4.7k',11)
    dot(770,y);line(770,y,770,y-58);box(762,y-88,16,30);text(700,y-103,'+5V_ISO');text(785,y-68,'4.7k',11)
    text(405,y+49,'Gate tied to 3V3; body diode orientation must follow S/D.',10)
i2c_channel(250,'SCL','PA15','Q3')
i2c_channel(440,'SDA','PB7','Q4')
notes(45,540,[
 'J4 LCD connector: GND=GND_LV, VCC=+5V_ISO, SCL=SCL_HV, SDA=SDA_HV.',
 'R16/R17: 4.7k LV pullups. R18/R19: 4.7k HV pullups.',
 'If the backpack already has HV pullups, omit R18/R19 after measuring the existing resistance.',
 'BSS138 SOT-23 common pinout: 1=G, 2=S, 3=D; verify chosen manufacturer before footprint assignment.',
 'C13: 100 nF and C14: 10 uF at J4 +5V/GND. Use short I2C wiring and start at 100 kHz.',
 'Discover actual PCF8574/PCF8574A I2C address and backpack LCD pin mapping in firmware.',
 'J5 optional SWD: 3V3 reference, GND, PA13/DIO, PA14/CLK, NRST; avoid duplicate target power.'
 ],12,26)
finish()

start('5. BOM, design limits and release checks','Complete signal/power connectivity is specified on sheets 1-4; final PCB footprints and mains protection require validation.')
rows=[
 ('Q1','BTA24-800CWRG, insulated TO-220, 800 V / 25 A','Verify pins; heatsink calculation + full-load temperature test'),
 ('U1 / U2','H11AA1 / MOC3052M, DIP-6','Maintain mains-to-LV barrier; U2 is random phase'),
 ('Q2 / U3','2N3904 / SN74LVC1G17DBVR','Verify B/C/E footprint; U3 requires the specified LVC family'),
 ('Q3 / Q4','2 x BSS138','I2C bidirectional translator; gate at 3V3'),
 ('R1 / R2 / R3','200R 1 W x2 / 330R 0.5 W','Gate limiting: pulse-rated / flameproof resistors'),
 ('R4 / C1','100R 2 W / 47 nF X2 305 VAC','Series snubber; tune after inductive-load measurements'),
 ('R5 / R6','15k 2 W x2, flameproof, HV-rated','Zero-cross input; thermal clearance around resistors'),
 ('R7 / R8 / R9 / R10','10k / 180R 0.5 W / 1k / 10k','ZC pullup / opto LED / Q2 base / Q2 pulldown'),
 ('R11 / R12 / R13','1k / 10k / 1k','Hall series / Hall pullup / ADC series'),
 ('R14..R19 / RV2','10k x2; 4.7k x4 / 10k linear pot','Buttons / I2C / speed setting'),
 ('C2..C14','100 nF x8; 10 uF x3; 47 uF x1; 100 pF x1','Electrolytics >=10 V; C11/C12 are button filters'),
 ('RV1 / PS1','275 VAC MOV / certified isolated 5 V 1 A PSU','MOV thermal protection required; coordinate surge/fuse ratings'),
 ('F1 / K0 / HS1','Motor fuse / two-pole cutoff / heatsink','Validate fuse; HS1 <=2 C/W initial target, thermal test required'),
]
for i,(ref,val,note) in enumerate(rows):
    y=140+i*27
    text(40,y,ref,10,bold=True);text(155,y,val,10);text(560,y,note,10)
line(35,501,1085,501)
notes(40,530,[
 'RELEASE CHECKS: no mains bench test until protective earth, insulation, fuse and enclosure have been reviewed.',
 'Target >=8 mm separation between mains and LV copper; final creepage/clearance depends on applicable standard,',
 'pollution, overvoltage category and materials. This target is not a compliance certification. No copper under barrier.',
 'Gate timing: ZC edges are not exact zero. Calibrate pulse-window timing; inhibit pulses on ZC loss or implausible period.',
 'Power-up stopped; explicit START required. STOP cancels timers. Watchdog/reset must leave PA5 LOW.',
 'Hall loss/stall/overspeed must stop firing; tune startup grace and threshold for actual spindle speed and magnet count.',
 'Retain independent motor overload/thermal protection. No current-measurement channel is included in this revision.',
 'Triac may fail short; K0 must interrupt motor power independently. Preserve magnetic-base interlocks.',
 'Use differential/isolated mains-rated measurement equipment: never attach an ordinary scope ground clip to mains.'
 ],11,21)
finish();c.save()

sources=[
 ('BTA24','https://www.st.com/resource/en/datasheet/bta24.pdf'),
 ('MOC3052M','https://www.onsemi.com/pdf/datasheet/moc3052m-d.pdf'),
 ('H11AA1','https://www.vishay.com/docs/83608/h11aa1.pdf'),
 ('SN74LVC1G17','https://www.ti.com/lit/ds/symlink/sn74lvc1g17.pdf'),
 ('STM32G431CB','https://www.st.com/resource/en/datasheet/stm32g431cb.pdf'),
 ('BSS138','https://www.onsemi.com/pdf/datasheet/bss138-d.pdf')]
(ROOT/'sources.txt').write_text('\n'.join(f'{n}: {u}' for n,u in sources),encoding='utf-8')
with (ROOT/'SmartDrill_G431_BOM.csv').open('w',newline='',encoding='utf-8-sig') as f:
    wr=csv.writer(f);wr.writerow(['Reference','Value / part','Selection notes']);wr.writerows(rows)
connections={
 'AC_L':['J1.L','F1.1','PS1.L (manufacturer-required protection)','ORIGINAL_MAGNET_SUPPLY.L'],
 'AC_N':['J1.N','K0.N_IN','PS1.N','ORIGINAL_MAGNET_SUPPLY.N'],
 'PE':['J1.PE','DRILL_CHASSIS.PE','PS1.PE (if required)'],
 'L_FUSED':['F1.2','K0.L_IN'],
 'L_SW':['K0.L_OUT','M1.1','RV1.1','R5.1'],
 'N_SW':['K0.N_OUT','Q1.1_A1','R3.2','C1.2','RV1.2','R6.1'],
 'MOTOR_A2':['M1.2','Q1.2_A2','R1.1','R4.1'],
 'OPTO_LIMIT_MID':['R1.2','R2.1'],
 'OPTO_MT':['R2.2','U2.6'],
 'GATE':['U2.4','Q1.3_G','R3.1'],
 'SNUBBER_MID':['R4.2','C1.1'],
 'ZC_LED_1':['R5.2','U1.1'],
 'ZC_LED_2':['R6.2','U1.2'],
 '+5V_ISO':['PS1.+5V','BOARD_USB.VBUS','R8.1','J3.VCC','R12.1','R18.1','R19.1','J4.VCC','C2.1','C3.+','C4.1','C5.+','C8.1','C9.+','C13.1','C14.+'],
 'GND_LV':['PS1.0V','BOARD_USB.GND','BOARD.GND','U1.4','Q2.E','R10.2','J3.GND','U3.3_GND','J4.GND','RV2.LOW','SW1.2','SW2.2','J5.GND','C2.2','C3.-','C4.2','C5.-','C6.2','C7.2','C8.2','C9.-','C10.2','C11.2','C12.2','C13.2','C14.-'],
 '3V3':['BOARD.3V3','R7.1','U3.5_VCC','C7.1','RV2.HIGH','R14.1','R15.1','Q3.G','Q4.G','R16.1','R17.1','J5.3V3_REFERENCE'],
 'ZERO_CROSS':['U1.5','R7.2','BOARD.PA0'],
 'FIRE':['BOARD.PA5','R9.1'],
 'Q2_BASE':['R9.2','Q2.B','R10.1'],
 'OPTO_LED_ANODE':['R8.2','U2.1'],
 'OPTO_LED_CATHODE':['U2.2','Q2.C'],
 'HALL_5V':['J3.OUT','R11.1','R12.2'],
 'HALL_FILTER':['R11.2','U3.2_A','C6.1'],
 'HALL_3V3':['U3.4_Y','BOARD.PA3'],
 'SPEED_WIPER':['RV2.WIPER','R13.1'],
 'SPEED_ADC':['R13.2','C10.1','BOARD.PA1'],
 'START_N':['BOARD.PB0','R14.2','C11.1','SW1.1'],
 'STOP_N':['BOARD.PB1','R15.2','C12.1','SW2.1'],
 'SCL_LV':['BOARD.PA15','R16.2','Q3.S'],
 'SDA_LV':['BOARD.PB7','R17.2','Q4.S'],
 'SCL_HV':['Q3.D','R18.2','J4.SCL'],
 'SDA_HV':['Q4.D','R19.2','J4.SDA'],
 'SWDIO':['BOARD.PA13','J5.DIO'],
 'SWCLK':['BOARD.PA14','J5.CLK'],
 'RESET_N':['BOARD.NRST','J5.NRST'],
 'NC':['U1.3','U1.6_BASE','U2.3','U2.5','U3.1','BOARD.PC6','BOARD.PB8'],
}
seen={}
for net,endpoints in connections.items():
    for endpoint in endpoints:
        assert endpoint not in seen,(endpoint,net,seen.get(endpoint))
        seen[endpoint]=net
for ref in [f'R{i}' for i in range(1,20)]+[f'C{i}' for i in range(1,15)]:
    assert sum(e.startswith(ref+'.') for e in seen)==2,ref
assert all(not e.startswith(('Q1.','J1.','M1.')) for e in connections['GND_LV'])
with (ROOT/'SmartDrill_G431_connections.csv').open('w',newline='',encoding='utf-8-sig') as f:
    wr=csv.writer(f);wr.writerow(['Net','Terminal']);wr.writerows((net,e) for net,ends in connections.items() for e in ends)
print('Created 5-page PDF, 5 editable SVG sheets, BOM CSV and sources.')


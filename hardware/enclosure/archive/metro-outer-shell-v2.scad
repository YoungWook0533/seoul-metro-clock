// Metro Clock replacement outer shell v2, in millimeters.
// Assembly coordinates: X left/right, Y up, Z front to rear.

part = "shell"; // "shell", "tray-gauge", or "button-gauge"

/* [Case and original tray] */
outer_width = 35.5;
outer_height = 40;
outer_depth = 45;
front_setback = 8; // The front top edge is set back from the front bottom edge.
outer_corner = 2;
front_chamfer = 0.8;

tray_width = 32.8;
tray_height = 37.1;
tray_corner = 0.8;
slide_clearance = 0.25; // Each side of the tray.
rear_leadin = 0.35;
bezel_thickness = 2.25;
gauge_depth = 14;

/* [LCD] */
screen_width = 28.5;
screen_height = 28.3;
screen_y = 1.15;
panel_width = 32.2;
panel_height = 34.4;
panel_y = 0.15;
screen_lip = 0.7;
ribbon_width = 12;

/* [Original underside screws] */
screw_pitch = 19.5;
screw_from_rear = 7.2;
screw_clearance = 2.4;
screw_head = 4.2;
screw_recess = 0.7;
screw_adjustment = 3.2; // Total front-to-back travel within each screw slot.

/* [Top DIP-4 switch] */
button_x = 0;
button_z = 23;
button_body = [6, 6]; // Nominal button is 6 x 6 x 5 mm including its plunger.
button_body_clearance = 0.17; // Per side.
button_case_height = 3.5; // Switch body excluding plunger and terminals.
button_hole = 4.0;
button_skin = 0.7; // Exterior roof plane to the top of the switch body.
finger_recess = 0.2;
clip_clearance = 0.12; // Gap between the seated switch base and the latches.
clip_width = 4.4;
clip_wall = 0.85;
clip_bite = 0.22;
clip_ramp = 0.9;

$fn = 48;
eps = 0.025;
inner_width = tray_width + 2 * slide_clearance;
inner_height = tray_height + 2 * slide_clearance;
wall = (outer_height - inner_height) / 2;
tilt = atan(front_setback / outer_height);

module round_rect(w, h, r) {
    offset(r = r) square([w - 2*r, h - 2*r], center = true);
}

module profile(w, h, r, z, thickness = eps) {
    translate([0, 0, z])
        linear_extrude(height = thickness) round_rect(w, h, r);
}

module along_front() {
    multmatrix([[1,0,0,0], [0,1,0,0], [0,front_setback/outer_height,1,front_setback/2], [0,0,0,1]])
        children();
}

module outside() {
    hull() {
        along_front() profile(outer_width - 2*front_chamfer, outer_height - 2*front_chamfer,
                              outer_corner - front_chamfer, 0);
        along_front() profile(outer_width, outer_height, outer_corner, front_chamfer);
        profile(outer_width, outer_height, outer_corner, outer_depth-eps);
    }
}

module tray_void() {
    along_front() profile(inner_width, inner_height, tray_corner, bezel_thickness, outer_depth);
}

module rear_entry() {
    hull() {
        profile(inner_width, inner_height, tray_corner, outer_depth - rear_leadin);
        profile(inner_width + 2*rear_leadin, inner_height + 2*rear_leadin,
                tray_corner + rear_leadin, outer_depth);
    }
}

module screen_void() {
    along_front() {
        translate([0, screen_y, 0])
            profile(screen_width, screen_height, 0.4, -eps, bezel_thickness + 2*eps);
        translate([0, panel_y, 0])
            profile(panel_width, panel_height, 0.5, screen_lip, bezel_thickness - screen_lip + eps);
        translate([-ribbon_width/2, panel_y + panel_height/2 - eps, screen_lip])
            cube([ribbon_width, 1.2, bezel_thickness - screen_lip + eps]);
    }
}

module screw_slot(diameter, height, exit_diameter) {
    hull() {
        for (end = [-1, 1])
            translate([0, end*screw_adjustment/2, 0])
                cylinder(d1 = diameter, d2 = exit_diameter, h = height);
    }
}

module underside_screws() {
    for (x = [-screw_pitch/2, screw_pitch/2])
        translate([x, -outer_height/2-eps, outer_depth-screw_from_rear]) rotate([-90,0,0]) {
            screw_slot(screw_clearance, wall + 2*eps, screw_clearance);
            screw_slot(screw_head, screw_recess + eps, screw_clearance);
        }
}

// Local Z points into the enclosure. Push the switch up from the inside,
// with its terminal pairs facing +/-X and the clips engaging the other edges.
module button_clips(case_height = button_case_height) {
    half_body = button_body[1]/2;
    inner = half_body + button_body_clearance;
    latch = button_skin + case_height + clip_clearance;
    for (side = [-1, 1])
        multmatrix([[0,0,1,-clip_width/2], [side,0,0,0], [0,1,0,0], [0,0,0,1]])
            linear_extrude(height = clip_width)
                polygon([
                    [inner, wall-eps],
                    [inner+clip_wall, wall-eps],
                    [inner+clip_wall, latch+clip_ramp],
                    [inner, latch+clip_ramp],
                    [half_body-clip_bite, latch],
                    [inner, latch]
                ]);

    // Center stops engage the switch body between each pair of solder legs.
    for (side = [-1, 1])
        translate([side*(button_body[0]/2+button_body_clearance+0.5), 0,
                   (wall-eps+button_skin+case_height)/2])
            cube([1, 1.3, button_skin+case_height-wall+eps], center = true);
}

module button_void() {
    seat = button_body + 2*[button_body_clearance, button_body_clearance];
    translate([0, 0, -eps]) cylinder(d = button_hole, h = wall + 2*eps);
    translate([0, 0, -eps])
        cylinder(d1 = 9.2, d2 = 8.6, h = finger_recess + eps);
    translate([-seat[0]/2, -seat[1]/2, button_skin])
        cube([seat[0], seat[1], wall-button_skin+eps]);
}

module on_roof() {
    translate([button_x, outer_height/2, button_z]) rotate([90, 0, 0]) children();
}

module shell() {
    assert(wall > button_skin && button_skin > finger_recess && outer_width > inner_width + 2*rear_leadin &&
           outer_height > inner_height + 2*rear_leadin &&
           gauge_depth > screw_from_rear + screw_adjustment/2 + screw_head/2);
    difference() {
        union() {
            difference() { outside(); tray_void(); rear_entry(); }
            on_roof() button_clips();
        }
        screen_void();
        underside_screws();
        on_roof() button_void();
    }
}

module tray_gauge() {
    translate([0, 0, gauge_depth-outer_depth])
        difference() {
            profile(outer_width, outer_height, outer_corner,
                    outer_depth-gauge_depth, gauge_depth);
            tray_void();
            rear_entry();
            underside_screws();
        }
}

// Top coupon, left to right: clips for 3.3, 3.5, and 3.7 mm switch bodies.
module button_gauge() {
    difference() {
        union() {
            translate([-20, -7, 0]) cube([40, 14, wall]);
            for (i = [-1:1])
                translate([12*i, 0, 0]) button_clips(button_case_height + 0.2*i);
        }
        for (i = [-1:1])
            translate([12*i, 0, 0]) button_void();
    }
}

if (part == "shell")
    translate([0, 0, -front_setback/2*cos(tilt)]) rotate([-tilt, 0, 0]) shell();
else if (part == "tray-gauge") tray_gauge();
else if (part == "button-gauge") button_gauge();

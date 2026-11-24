// Metro Clock replacement outer shell, in millimeters.
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
screen_lip = 0.95;
ribbon_width = 12;

/* [Original underside screws] */
screw_pitch = 19.5;
screw_from_rear = 9;
screw_clearance = 2.4;
screw_head = 4.2;
screw_recess = 0.7;
screw_adjustment = 1.6; // Total front-to-back travel within each screw slot.

/* [Top DIP-4 switch] */
button_x = 0;
button_z = 23;
button_body = [6, 6]; // Nominal button is 6 x 6 x 5 mm including its plunger.
button_body_clearance = 0.20;
button_hole = 4.0;
button_skin = 1.1;
guide_depth = 1.25;

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

// Button geometry uses Z=0 at the exterior and positive Z into the housing.
// The rails leave the two terminal sides open.
module button_rails() {
    seat_depth = button_body[1] + 2*button_body_clearance;
    rail = 1.25;
    for (direction = [-1, 1])
        translate([0, direction * (seat_depth + rail)/2, wall + (guide_depth - eps)/2])
            cube([button_body[0] + 2*button_body_clearance + rail, rail, guide_depth + eps], center = true);
}

module button_void(hole) {
    seat = [button_body[0] + 2*button_body_clearance,
            button_body[1] + 2*button_body_clearance];
    translate([0, 0, -eps]) cylinder(d = hole, h = wall + guide_depth + 2*eps);
    translate([-seat[0]/2, -seat[1]/2, button_skin])
        cube([seat[0], seat[1], wall + guide_depth - button_skin + eps]);
}

module on_roof() {
    translate([button_x, outer_height/2, button_z]) rotate([90, 0, 0]) children();
}

module shell() {
    assert(wall > button_skin && outer_width > inner_width + 2*rear_leadin &&
           outer_height > inner_height + 2*rear_leadin &&
           gauge_depth > screw_from_rear + screw_adjustment/2 + screw_head/2);
    difference() {
        union() {
            difference() { outside(); tray_void(); rear_entry(); }
            on_roof() button_rails();
        }
        screen_void();
        underside_screws();
        on_roof() button_void(button_hole);
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

// The three holes are 3.6, 4.0, and 4.4 mm from left to right.
module button_gauge() {
    difference() {
        union() {
            translate([-20, -7, 0]) cube([40, 14, wall]);
            for (x = [-12, 0, 12])
                translate([x, 0, 0]) button_rails();
        }
        for (i = [0:2])
            translate([(i-1)*12, 0, 0]) button_void(button_hole + (i-1)*0.4);
    }
}

if (part == "shell")
    translate([0, 0, -front_setback/2*cos(tilt)]) rotate([-tilt, 0, 0]) shell();
else if (part == "tray-gauge") tray_gauge();
else if (part == "button-gauge") button_gauge();

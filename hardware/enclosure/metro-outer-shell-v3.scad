// Metro Clock replacement outer shell v3, in millimeters.
// Assembly coordinates: X left/right, Y up, Z front to rear.

part = "shell"; // "shell", "retainer", or "button-gauge"

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
button_socket = 6.5;
button_hole = 4.2;
button_skin = 0.8;
finger_recess = 0.2;
track_side = 6.45; // Inside face of the rail's side wall.
track_floor = 6.55; // From the exterior roof to the sliding ledge.
track_edge = 4.55;
rail_web = 1.4;
retainer_thickness = 2.2; // Gives 4.35 mm between roof datum and bearing face.
track_front = -5.4;
track_rear = 10.5;
detent_y = -0.5;

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

// Roof coordinates: X left/right, Y front/rear, Z inward.
module roof_rail_section(y, x0, x1, z0, z1) {
    translate([x0,y,z0]) cube([x1-x0,eps,z1-z0]);
}

module roof_mount() {
    base = wall-eps;
    back = track_side+rail_web;
    floor = track_floor+rail_web;
    for (side = [0,1])
        mirror([side,0,0])
            union() {
                // Both rail features grow inward from the roof as layers advance.
                hull() {
                    roof_rail_section(track_front-9, track_edge, back, base, base+0.08);
                    roof_rail_section(track_front, track_edge, back, track_floor, floor);
                }
                hull() {
                    roof_rail_section(track_front, track_edge, back, track_floor, floor);
                    roof_rail_section(track_rear, track_edge, back, track_floor, floor);
                }
                hull() {
                    roof_rail_section(track_front-9, track_side, back, base, base+0.08);
                    roof_rail_section(track_front, track_side, back, base, floor);
                    roof_rail_section(track_rear, track_side, back, base, floor);
                }
            }
    // Shallow socket locates the body. Its leading edge ramps up from the roof.
    hull() {
        translate([-4.6,-7.3,wall-eps]) cube([9.2,eps,0.08]);
        translate([-4.6,-4.3,wall-eps]) cube([9.2,eps,1.38]);
        translate([-4.6,6.3,wall-eps]) cube([9.2,eps,1.38]);
    }
}

// The point closes the hole gradually as the layers approach the rear.
module pointed_hole(d) {
    r=d/2;
    hull() {
        circle(d=d);
        polygon([[-r/sqrt(2),r/sqrt(2)],[r/sqrt(2),r/sqrt(2)],[0,r*sqrt(2)]]);
    }
}

module track_detent(y, side) {
    translate([0,y,0]) mirror([side,0,0]) children();
}

module roof_void() {
    translate([0,0,-eps])
        linear_extrude(height = wall + 2*eps) pointed_hole(button_hole);
    translate([0,0,-eps])
        linear_extrude(height = finger_recess+eps) pointed_hole(8.4);
    // Keep both terminal sides open and ramp the trailing pocket wall for printing.
    translate([-track_side-eps,0,0])
        multmatrix([[0,0,1,0], [1,0,0,0], [0,1,0,0], [0,0,0,1]])
            linear_extrude(height = 2*(track_side+eps))
                polygon([[-button_socket/2,button_skin],
                         [button_socket/2,button_skin],
                         [button_socket/2+3.7,button_skin+2.6],
                         [-button_socket/2,button_skin+2.6]]);
    for (side = [0,1])
        track_detent(detent_y, side)
            translate([0,0,3.9]) linear_extrude(height = 3.1)
                polygon([[track_side-eps,-1.5],[track_side+0.6,0],[track_side-eps,1.5]]);
}

// Print the retainer flat. Its three prongs leave continuous channels for the pins.
module retainer() {
    tine_edge = track_edge+0.08;
    tine_outer = track_side-0.25;
    linear_extrude(height = retainer_thickness)
        union() {
            translate([-1.25,-4.9]) square([2.5,17.3]);
            for (side = [0,1])
                mirror([side,0])
                    union() {
                        translate([tine_edge,-4.9]) square([tine_outer-tine_edge,17.3]);
                        translate([0,detent_y])
                            polygon([[tine_outer-0.15,-1.35],[tine_outer+0.5,0],
                                     [tine_outer-0.15,1.35]]);
                        // Broader root reduces the stress concentration as each arm flexes.
                        polygon([[3.4,10.5],[tine_edge,8.6],[tine_edge,11.9],[3.4,11.9]]);
                    }
            translate([-tine_outer,10.5]) square([2*tine_outer,3.3]);
        }
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
            on_roof() roof_mount();
        }
        screen_void();
        underside_screws();
        on_roof() roof_void();
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

// Coupon uses the same tilt and print direction as the case.
module button_gauge() {
    difference() {
        union() {
            translate([-9,-15,0]) cube([18,27,wall]);
            roof_mount();
        }
        roof_void();
    }
}

module front_down() {
    translate([0, 0, -front_setback/2*cos(tilt)]) rotate([-tilt, 0, 0]) children();
}

if (part == "shell")
    front_down() shell();
else if (part == "retainer")
    retainer();
else if (part == "button-gauge")
    translate([0,0,15*cos(tilt)]) union() {
        rotate([90-tilt,0,0]) button_gauge();
        translate([-9,-8,-15*cos(tilt)]) cube([18,13,2]);
    }


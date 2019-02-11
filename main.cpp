#include <iostream>
#include <fstream>
#include <cmath> //M_PI
#include <string>

constexpr float PRINT_SPEED = 1200;
constexpr float LAYER_HEIGHT = 0.2;
constexpr float NOZZLE_WIDTH = 0.4;
constexpr float FILAMENT_RADIUS = 1.75;



class printer{
private:
    float m_x;
    float m_y;
    float m_z;
    float m_e; //extrusion
    float m_f; //speed

    float m_layer_height;
    float m_print_speed;
    float m_travel_speed;
    float m_nozzle_width;
    float m_filament_radius;

public:
    printer(float layer_height, float print_speed, float travel_speed, float nozzle_width, float filament_radius): m_x(0) ,m_y(0), m_z(layer_height),
        m_e(), m_f(), m_layer_height(layer_height), m_print_speed(print_speed), m_travel_speed(travel_speed),
        m_nozzle_width(nozzle_width), m_filament_radius(filament_radius)
    {}

    float get_x() const{
        return m_x;
    }

    float get_y() const{
        return m_y;
    }

    float get_z() const{
        return m_z;
    }

    float get_radius() const{
        return m_filament_radius;
    }

    float get_layer_height() const{
        return m_layer_height;
    }

    float extrusion(double length) const{
        return m_layer_height * m_nozzle_width * length * 4. / (m_filament_radius*m_filament_radius*M_PI);
    }

    std::string go_to(float x, float y, float z = -1., bool extr = false){
        const std::string G = extr ? "G1" : "G0";

        const double dx = std::abs(double(m_x - x));
        const double dy = std::abs(double(m_y - y));
        const double dz = z < 0. ? 0. : std::abs(double(m_z - z));

        m_e += extr ? extrusion(std::sqrt(dx*dx + dy*dy + dz*dz)) : 0.;
        m_x = x;
        m_y = y;
        m_z = z < 0. ? m_z : z;

        auto ans = G + coordinates() + " F";
        ans.append(std::to_string(extr ? m_print_speed : m_travel_speed));
        ans.append("\n");
        return ans;
    }

    std::string move(float dx, float dy, float dz, bool extr = true){
        const std::string G = extr ? "G1" : "G0";

        m_x += dx;
        m_y += dy;
        m_z += dz;
        m_e += extr ? extrusion(std::sqrt(dx*dx + dy*dy + dz*dz)) : 0.;

        auto ans = G + coordinates() + " F";
        ans.append(std::to_string(extr ? m_print_speed : m_travel_speed));
        ans.append("\n");
        return ans;
    }


    std::string coordinates() const{
        return " X" + std::to_string(m_x) + " Y" + std::to_string(m_y) + " Z" +
                std::to_string(m_z) + " E" + std::to_string(m_e);
    }


};





/*float square_layer(std::ostream& stream, float side, float cur_extr, float start_x, float start_y, float start_z){
    const float extr = extrusion(side);

    //first
    cur_extr += extr;
    stream<< "G1 X"<< start_x + side<< " Y"<< start_y<< " Z"<< start_z
          << " E"<< cur_extr<< " F"<< PRINT_SPEED<< std::endl;
    //second
    cur_extr += extr;
    stream<< "G1 X"<< start_x + side<< " Y"<< start_y + side<< " Z"<< start_z
          << " E"<< cur_extr<< " F"<< PRINT_SPEED<< std::endl;
    //third
    cur_extr += extr;
    stream<< "G1 X"<< start_x<< " Y"<< start_y + side<< " Z"<< start_z
          << " E"<< cur_extr<< " F"<< PRINT_SPEED<< std::endl;
    //fourth
    cur_extr += extr;
    stream<< "G1 X"<< start_x + side<< " Y"<< start_y<< " Z"<< start_z
          << " E"<< cur_extr<< " F"<< PRINT_SPEED<< std::endl;

    return cur_extr;
}*/


void square_layer(std::ostream& stream, printer& p, float side){
    stream<< p.move(side, 0, 0);
    stream<< p.move(0, side, 0);
    stream<< p.move(-side, 0, 0);
    stream<< p.move(0, -side, 0);
}


//Y oriented zigzag
/*void zigzag(std::ostream& stream, printer& p, float length, float angle, float width){
    const float cos_angle = std::cos(angle);
    const float sin_angle = std::sin(angle);
    const float start_y = p.get_y();

    bool even = true;
    while(p.get_y() < start_y + length){
        stream<< p.move(even ? cos_angle*width : -cos_angle*width, sin_angle*width, 0.);
        even = !even;
    }
}*/


//fill with a zigzag pattern
//substract the necessary portion from the length to avoid overlaps
void zigzag_fill_horizontal(std::ostream& stream, printer& p, float length){
    stream<< p.move(p.get_radius()*2., p.get_radius()*2., 0, false);
    length -= 4. * p.get_radius();
    const float start_y = p.get_y();
    //stop when adding a line extrudes on an existing line
    float even = 1.; //1 when even, -1 when odd
    while(p.get_y() - start_y < length){
        //print a line
        stream<< p.move(length * even, 0, 0);
        //move up to next line
        stream<< p.move(0, p.get_radius()*2, 0, false);

        even = -even;
    }

}

void zigzag_fill_vertical(std::ostream& stream, printer& p, float length){
    stream<< p.move(p.get_radius()*2., p.get_radius()*2., 0, false);
    length -= 4. * p.get_radius();
    const float start_x = p.get_x();
    //stop when adding a line extrudes on an existing line
    float even = 1.; //1 when even, -1 when odd
    while(p.get_x() - start_x < length){
        //print a line
        stream<< p.move(0, length*even, 0);
        //move up to next line
        stream<< p.move(p.get_radius()*2., 0, 0, false);

        even = -even;
    }
}


void print_square(std::ostream& stream, printer& p, float side, unsigned nb_layers){
    const float start_x = p.get_x(), start_y = p.get_y();
    bool even = true;
    for(unsigned i=0; i < nb_layers; i++){
        square_layer(stream, p, side);
        if(even){
            zigzag_fill_horizontal(stream, p, side);
            stream<< p.go_to(start_x, start_y);
        }
        else{
            zigzag_fill_vertical(stream, p, side);
            stream<< p.go_to(start_x, start_y);
        }
        stream<< p.move(0, 0, p.get_layer_height(), false);
        even = !even;
    }
}

void circle_layer(std::ostream& stream, printer& p, float radius, unsigned nb_segs, float center_x, float center_y){
    const float angle = M_PI * 2. / float(nb_segs);
    //go to first point, without printing
    stream<< p.go_to(center_x + std::cos(0) * radius, center_y + std::sin(0) * radius);
    for(unsigned i=1; i < nb_segs; i++){
        //print to the next point
        stream<< p.go_to(center_x + std::cos(i*angle) * radius, center_y + std::sin(i*angle) * radius, -1., true);
    }
    //print the last segment
    stream<< p.go_to(center_x + std::cos(0) * radius, center_y + std::sin(0) * radius, -1, true);
}


void print_cylinder(std::ostream& stream, printer& p, float radius, unsigned nb_segs, float center_x, float center_y, unsigned nb_layers){
    for(unsigned i=0; i < nb_layers; i++){
        circle_layer(stream, p, radius, nb_segs, center_x, center_y);
        stream<< p.move(0, 0, p.get_layer_height(), false);
    }
}



int main(){
    std::ofstream file("output.gcode");

    printer p(LAYER_HEIGHT, PRINT_SPEED, 3000., NOZZLE_WIDTH, FILAMENT_RADIUS);
    file<< p.go_to(80, 80, LAYER_HEIGHT, false);
    print_cylinder(file, p, 20., 30, 80., 80., 500);
    return 0;
}


#include <iostream>
#include <fstream>
#include <cmath> //M_PI
#include <string>
#include <vector>

constexpr float PRINT_SPEED = 1200;
constexpr float TRAVEL_SPEED = 3000;
constexpr float LAYER_HEIGHT = 0.2;
constexpr float NOZZLE_WIDTH = 0.4;
constexpr float FILAMENT_RADIUS = 1.75;


float intersection_with_fixed_y(float a, float b, float c, float y){
    return (-b*y - c) / a;
}

float intersection_with_fixed_x(float a, float b, float c, float x){
    return (-a*x - c) / b;
}


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
        return m_nozzle_width;
    }

    float get_layer_height() const{
        return m_layer_height;
    }

    float extrusion(double length) const{
        return m_layer_height * m_nozzle_width * length * 4. / (m_filament_radius*m_filament_radius*M_PI);
    }

    std::string header() const{
        return "G21\n"
                "G90\n"
                "G28\n"
                "M140 S60\n"
                "M105\n"
                "M190 S60\n"
                "M104 S200\n"
                "M105\n"
                "M109 S200\n"
                "G92 E0\n"
                "G1 E10 F1200\n"
                "G92 E0\n";
    }

    std::string end_print(){
        return go_to(0, 0, std::min(240., get_z() + 10.), false);
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



void square_layer(std::ostream& stream, printer& p, float side){
    stream<< p.move(side, 0, 0);
    stream<< p.move(0, side, 0);
    stream<< p.move(-side, 0, 0);
    stream<< p.move(0, -side, 0);
}



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


void print_cylinder(std::ostream& stream, printer& p, float radius, unsigned nb_segs,
                    float center_x, float center_y, unsigned nb_layers){
    //find the number of inner circles
    std::vector<unsigned> radiuss;
    float current_radius = radius;
    while(current_radius > p.get_radius()){
        radiuss.push_back(current_radius);
        current_radius -= p.get_radius()*2.;
    }
    //actually compute the circles
    bool even = true;
    for(unsigned i=0; i < nb_layers; i++){
        if(even){ //outside to inside
            for(int j=0; j < int(radiuss.size()); j++){
                circle_layer(stream, p, radiuss[j], nb_segs, center_x, center_y);
            }
        }
        else{ //inside to outside
            for(int j=radiuss.size()-1; j >= 0; j--){
                circle_layer(stream, p, radiuss[j], nb_segs, center_x, center_y);
            }
        }
        stream<< p.move(0, 0, p.get_layer_height(), false);
        even = !even;
    }
}


void circle_infill(std::ostream& stream, printer& p, float radius, float center_x, float center_y){
    //go to the first place to begin the infill
    p.go_to(center_x-radius, center_y);
    bool up = true;
    float current_x = p.get_x();
    bool finished = false;
    while(!finished){
        //print the line on y axis direction
        //find the intersection with the circle towards down
        //we use Pythagore
        //y = sqrt(r²-x²)
        //x = |center_x - xp|
        //yp = center_y +- y
        {
            const float x = std::abs(center_x - current_x);
            const float y = std::sqrt(radius*radius - x*x <= 0. ? 0. : radius*radius - x*x);
            const float yp = up ? center_y + y : center_y - y;
            stream<< p.go_to(current_x, yp, -1, true);
        }
        //compute position on next line
        {
            current_x += p.get_radius();
            finished = current_x >= center_x + radius;
            if(!finished){
                const float x = std::abs(center_x - current_x);
                const float y = std::sqrt(radius*radius - x*x <= 0. ? 0. : radius*radius - x*x);
                const float yp = up ? center_y + y : center_y - y;
                stream<< p.go_to(current_x, yp, -1, true);
            }
        }

        up = !up;
    }
}


void print_hemisphere(std::ostream& stream, printer& p, float radius, unsigned nb_segs,
                      float center_x, float center_y){
    float current_radius = std::sqrt(radius*radius - p.get_z()*p.get_z());
    while(current_radius >= p.get_radius()*2.){
        //compute this circle's radius
        //we use Pythagore:
        //if R is hemishpere radius, r is radius of this circle, z is layer height
        //r = sqrt(R^2 - z^2)
        circle_layer(stream, p, current_radius, nb_segs, center_x, center_y);
        circle_layer(stream, p, current_radius-p.get_radius(), nb_segs, center_x, center_y);
        circle_infill(stream, p, current_radius - 2.*p.get_radius(), center_x, center_y);
        p.move(0, 0, p.get_layer_height(), false);
        current_radius = std::sqrt(radius*radius - p.get_z()*p.get_z());
        //std::cout<< current_radius<< std::endl;
    }

}







void print_cube_embossing(std::ostream& stream, printer& p, float size, float spacing, float offset){
    const float start_x = p.get_x(), start_y = p.get_y();
    //const float end_x = start_x + size, end_y = start_y + size;

    //we use Pythagore to compute nb of diags
    //size*size + size*size = diag*diag
    //nb = floor(diag / spacingh)
    const unsigned nb_diags = static_cast<unsigned>(std::sqrt(2.)*size / spacing);
    const float delta_c = std::sqrt(1./2.)*spacing;
    bool up = true;
    float current_c = 0;

    //diagonal down and right (starting at 0;0, ending at size;size)
    for(int i=0; i < int(nb_diags); i++){
        if(up){
            const float y = intersection_with_fixed_x(1, 1, current_c, 0);
            if(y <= size){
                stream<< p.go_to(start_x, start_y + y, -1, true);
            }
            else{
                const float x = intersection_with_fixed_y(1, 1, current_c, size);
                stream<< p.go_to(start_x + x, start_y + size, -1, true);

            }
        }
        else{
            const float x = intersection_with_fixed_y(1, 1, current_c, 0);
            if(x <= size){
                stream<< p.go_to(start_x + x, start_y, -1, true);
            }
            else{
                const float y = intersection_with_fixed_x(1, 1, current_c, size);
                stream<< p.go_to(start_x + size, start_y + y, -1, true);
            }
        }
        up = !up;
        current_c -= delta_c;
    }


    stream<< p.go_to(start_x, start_y + size);
    //diagonal up and right (starting at 0;size, ending at size;0)
    up = true;
    current_c = -1.;
    for(int i=0; i < int(nb_diags); i++){
        if(up){
            const float x = intersection_with_fixed_y(-1, 1, current_c, size);
            if(x <= size){
                stream<< p.go_to(start_x + x, start_y + size, -1, true);
            }
            else{
                const float y = intersection_with_fixed_x(-1, 1, current_c, size);
                stream<< p.go_to(start_x + size, start_y + y, -1, true);
            }
        }
        else{
            const float y = intersection_with_fixed_x(-1, 1, current_c, 0);
            if(y >= 0){
                stream<< p.go_to(start_x, start_y + y, -1, true);
            }
            else{
                const float x = intersection_with_fixed_y(-1, 1, current_c, 0);
                stream<< p.go_to(start_x + x, start_y, -1, true);
            }
        }
        up = !up;
        current_c += delta_c;
    }

}


void print_cube_with_embossing(std::ostream& stream, printer& p, float size, unsigned nb_layers){
    for(int i=0; i < int(nb_layers); i++){
        square_layer(stream, p, 20.);
        stream<< p.move(p.get_radius(), p.get_radius(), 0, false);
        //infill
        print_cube_embossing(stream, p, size - 2.*p.get_radius(), 0.8, 0);
    }
}




int main(){
    std::ofstream file("output.gcode");

    printer p(LAYER_HEIGHT, PRINT_SPEED, TRAVEL_SPEED, NOZZLE_WIDTH, FILAMENT_RADIUS);
    file<< p.header();
    file<< p.go_to(0, 0, LAYER_HEIGHT, false);
    //circle_layer(file, p, 40, 10, 80, 80);
    print_cube_embossing(file, p, 40, 0.4, 0);
    //print_hemisphere(file, p, 10, 40, 80, 80);
    file<< p.end_print();
    return 0;
}


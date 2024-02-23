#include <algorithm>
#include <cmath>
#include <iostream>
#include <fstream>
#include <vector>
#include <sfml.hpp>
#include <json.hpp>
#include <lua.hpp>

using namespace std;
using namespace sf;
using namespace Json;

struct entity {
    int image;
    float x = 0;
    float y = 0;
    float distance;
};

namespace player {
    float player_x = 0;
    float player_y = 0;
    float player_r = 0;
    float speed = 3.5;
    float plane_x = 0;
    float plane_y = 0;
};

using namespace player;

const int SCREEN_WIDTH = 960;
const int SCREEN_HEIGHT = 480;
const int COLUMNS = 8;
const int ROWS = 8;
const int MAX_TEXTURES = 256;
const float PI = atan(1) * 4;
int TEXT_SIZE = SCREEN_WIDTH / 32;
int wall_tiles[ROWS][COLUMNS];
int floor_tiles[ROWS][COLUMNS];
int roof_tiles[ROWS][COLUMNS];
float buffer_z[SCREEN_WIDTH];

bool sort_struct(entity &a, entity &b) {
    return a.distance > b.distance;
}

vector<Image> images;
vector<SoundBuffer> sounds;
vector<entity> entities;

int main() {
    RenderWindow window(VideoMode(SCREEN_WIDTH, SCREEN_HEIGHT), "Framework");
    Event main_event;
	string err_path = "images/error.png";
	Texture err_texture;
	err_texture.loadFromFile(err_path);
	Image error_image = err_texture.copyToImage();
    ifstream file("levels/test.json");
    Value root;
    file >> root;
	Value doc_wall_tiles = root["wall_tiles"];
	for (int i = 0; i < ROWS; i++)
		for (int v = 0; v < COLUMNS; v++)
			wall_tiles[i][v] = doc_wall_tiles[i][v].asInt();
	Value doc_floor_tiles = root["floor_tiles"];
	for (int i = 0; i < ROWS; i++)
		for (int v = 0; v < COLUMNS; v++)
			floor_tiles[i][v] = doc_floor_tiles[i][v].asInt();
	Value doc_roof_tiles = root["roof_tiles"];
	for (int i = 0; i < ROWS; i++)
		for (int v = 0; v < COLUMNS; v++)
			roof_tiles[i][v] = doc_roof_tiles[i][v].asInt();
		
	Value doc_images = root["images"];
	
	for (int i = 0; i < doc_images.size(); i++) {
		string path = doc_images[i].asString();
		Texture texture;
		texture.loadFromFile(path);
		images.push_back(texture.copyToImage());
	}
	
	Value& doc_sounds = root["sounds"];
	
	for (int i = 0; i < doc_sounds.size(); i++) {
		string path = doc_sounds[i].asString();
		SoundBuffer buffer;
		buffer.loadFromFile(path);
		sounds.push_back(buffer);
	}
	
	Value& doc_entities = root["entities"];
	for (int i = 0; i < doc_entities.size(); ++i)
		entities.push_back(entity{doc_entities[i]["image"].asInt(), doc_entities[i]["x"].asFloat(), doc_entities[i]["y"].asFloat()});
    file.close();
    int move = 0;
    int rotate = 0;
    int hud = 0;
    Clock clock;
    player_x = 3.5;
    player_y = 3.5;

    while (window.isOpen()) {
        Time elapsed = clock.restart();
        float delta_time = elapsed.asSeconds();

        while (window.pollEvent(main_event)) {
            if (main_event.type == Event::Closed)
                window.close();
            else if (main_event.type == Event::KeyPressed) {
                switch (main_event.key.code) {
                case Keyboard::W:
                    move = 1;
                    break;
                case Keyboard::S:
                    move = -1;
                    break;
                case Keyboard::A:
                    rotate = 1;
                    break;
                case Keyboard::D:
                    rotate = -1;
                    break;
                case Keyboard::LShift:
                    speed = 5;
                    break;
                }
            }
			
			else if (main_event.type == Event::KeyReleased) {
                switch (main_event.key.code) {
                case Keyboard::W:
                case Keyboard::S:
                    move = 0;
                    break;
                case Keyboard::A:
                case Keyboard::D:
                    rotate = 0;
                    break;
                case Keyboard::LShift:
                    speed = 3.5;
                    break;
                }
            }
        }

        float point_x = cos(player_r);
        float point_y = sin(player_r);
        float new_x = player_x + move * speed * point_x * delta_time;
        float new_y = player_y + move * speed * point_y * delta_time;
        float new_r = player_r - rotate * speed * delta_time;
        float xo;
        float yo;
        if (wall_tiles[int(new_x)][int(player_y)] == -1)
            player_x = new_x;
        if (wall_tiles[int(player_x)][int(new_y)] == -1)
            player_y = new_y;
        player_r = fmod(new_r, PI * 2);
        float angle = player_r + PI / 2;
        plane_x = cos(angle);
        plane_y = sin(angle);
        window.clear();
        Image scene_image;
        scene_image.create(SCREEN_WIDTH, SCREEN_HEIGHT);

        for (int y = SCREEN_HEIGHT / 2; y < SCREEN_HEIGHT; y++) {
            float ray_point_x0 = point_x - plane_x;
            float ray_point_y0 = point_y - plane_y;
            float ray_point_x1 = point_x + plane_x;
            float ray_point_y1 = point_y + plane_y;
            int p = y - SCREEN_HEIGHT / 2;
            float player_z = SCREEN_HEIGHT / 2;
            float row_distance = player_z / p;
            float step_x = row_distance * (ray_point_x1 - ray_point_x0) / SCREEN_WIDTH;
            float step_y = row_distance * (ray_point_y1 - ray_point_y0) / SCREEN_WIDTH;
            float a = player_x + row_distance * ray_point_x0;
            float b = player_y + row_distance * ray_point_y0;

            for (int x = 0; x < SCREEN_WIDTH; x++) {
                int map_x = int(a);
                int map_y = int(b);
                a += step_x;
                b += step_y;
                int floor_texture_column = int((a - map_x) * 64) & (63); // Every floor texture will be 64x64
                int floor_texture_row = int((b - map_y) * 64) & (63);
                int floor_hit = floor_tiles[map_x][map_y];
                Color floor_color;
                if (floor_hit > -1 && floor_hit < images.size())
                    floor_color = images[floor_hit].getPixel(floor_texture_column, floor_texture_row);
				else
					floor_color = error_image.getPixel(floor_texture_column, floor_texture_row);
                scene_image.setPixel(x, y, floor_color);
                int roof_hit = roof_tiles[map_x][map_y];
                Color roof_color;
                if (roof_hit > -1 && roof_hit < images.size())
                    roof_color = images[roof_hit].getPixel(floor_texture_column, floor_texture_row);
				else
					roof_color = error_image.getPixel(floor_texture_column, floor_texture_row);
                scene_image.setPixel(x, SCREEN_HEIGHT - y, roof_color);
            }
        }

        for (int x = 0; x < SCREEN_WIDTH; x++) {
            float camera_x = x * 2 / float(SCREEN_WIDTH) - 1;
            float ray_point_x = point_x + plane_x * camera_x;
            float ray_point_y = point_y + plane_y * camera_x;
            float side_distance_x;
            float side_distance_y;
            float delta_distance_x = (ray_point_x == 0) ? 1e30 : abs(1 / ray_point_x);
            float delta_distance_y = (ray_point_y == 0) ? 1e30 : abs(1 / ray_point_y);
            float perpendicular_distance;
            int map_x = int(player_x);
            int map_y = int(player_y);
            int step_x;
            int step_y;

            if (ray_point_x < 0) {
                step_x = -1;
                side_distance_x = (player_x - map_x) * delta_distance_x;
            }
			
			else {
                step_x = 1;
                side_distance_x = (map_x + 1.0 - player_x) * delta_distance_x;
            }

            if (ray_point_y < 0) {
                step_y = -1;
                side_distance_y = (player_y - map_y) * delta_distance_y;
            }
			
			else {
                step_y = 1;
                side_distance_y = (map_y + 1.0 - player_y) * delta_distance_y;
            }

            int wall_hit = -1;
            int side;

            while (wall_hit == -1) {
                if (side_distance_x < side_distance_y) {
                    side_distance_x += delta_distance_x;
                    map_x += step_x;
                    side = 0;
                }
				
				else {
                    side_distance_y += delta_distance_y;
                    map_y += step_y;
                    side = 1;
                }

                if (wall_tiles[map_x][map_y] > -1)
                    wall_hit = wall_tiles[map_x][map_y];
            }

            if (side == 0)
                perpendicular_distance = side_distance_x - delta_distance_x;
            else
                perpendicular_distance = side_distance_y - delta_distance_y;
            int line_height = int(SCREEN_HEIGHT / perpendicular_distance);
            int draw_start = SCREEN_HEIGHT / 2 - line_height / 2;
            if (draw_start < 0)
                draw_start = 0;
            int draw_end = SCREEN_HEIGHT / 2 + line_height / 2;
            if (draw_end >= SCREEN_HEIGHT)
                draw_end = SCREEN_HEIGHT - 1;
            float wall_x;
            if (side == 0)
                wall_x = player_y + perpendicular_distance * ray_point_y;
            else
                wall_x = player_x + perpendicular_distance * ray_point_x;
            wall_x -= floor(wall_x);
            int wall_texture_column = int(wall_x * 64);

            for (int y = draw_start; y < draw_end; y++) {
                float texture_y = float(y - draw_start) / float(draw_end - draw_start);
                int wall_texture_row = int(texture_y * 64);
                Color color;
                if (wall_hit > -1 && wall_hit < images.size())
                    color = images[wall_hit].getPixel(wall_texture_column, wall_texture_row);
				else
					color = error_image.getPixel(wall_texture_column, wall_texture_row);
                scene_image.setPixel(x, y, color);
            }

            buffer_z[x] = perpendicular_distance;
        }

        for (int i = 0; i < entities.size(); i++)
            entities[i].distance = ((player_x - entities[i].x) * (player_x - entities[i].x) + (player_y - entities[i].y) * (player_y - entities[i].y));

        sort(entities.begin(), entities.end(), sort_struct);

        for (int i = 0; i < entities.size(); i++) {
            float entity_x = entities[i].x - player_x;
            float entity_y = entities[i].y - player_y;
            float i_dont_know = 1.0 / (plane_x * point_y - plane_y * point_x);
            float transform_x = i_dont_know * (point_y * entity_x - point_x * entity_y);
            float transform_y = i_dont_know * (-plane_y * entity_x + plane_x * entity_y);
            int sprite_screen_x = int((SCREEN_WIDTH / 2) * (1 + transform_x / transform_y));
            int sprite_height = abs(int(SCREEN_HEIGHT / transform_y));
            int draw_start_y = -sprite_height / 2 + SCREEN_HEIGHT / 2;
            if (draw_start_y < 0)
                draw_start_y = 0;
            int draw_end_y = sprite_height / 2 + SCREEN_HEIGHT / 2;
            if (draw_end_y >= SCREEN_HEIGHT)
                draw_end_y = SCREEN_HEIGHT - 1;
            int sprite_width = abs(int(SCREEN_HEIGHT / transform_y));
            int draw_start_x = -sprite_width / 2 + sprite_screen_x;
            if (draw_start_x < 0)
                draw_start_x = 0;
            int draw_end_x = sprite_width / 2 + sprite_screen_x;
            if (draw_end_x > SCREEN_WIDTH)
                draw_end_x = SCREEN_WIDTH;

            for (int stripe = draw_start_x; stripe < draw_end_x; stripe++) {
                int texture_x = int(256 * (stripe - (-sprite_width / 2 + sprite_screen_x)) * 64 / sprite_width) / 256;
                if (transform_y > 0 && transform_y < buffer_z[stripe])
                    for (int y = draw_start_y; y < draw_end_y; y++) {
                        int d = y * 256 - SCREEN_HEIGHT * 128 + sprite_height * 128;
                        int texture_y = ((d * 64) / sprite_height) / 256;
                        Color color = images[entities[i].image].getPixel(texture_x, texture_y);
                        if (color != Color(255, 0, 255))
                            scene_image.setPixel(stripe, y, color);
                    }
            }
        }

        Texture scene_texture;
        scene_texture.loadFromImage(scene_image);
        Sprite scene_sprite(scene_texture);
        window.draw(scene_sprite);
        window.display();
    }
	
    return 0;
}

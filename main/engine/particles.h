#ifndef PARTICLE_H
#define PARTICLE_H

#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include "engine/ctetris.h"
#include "game.h"
#include "engine/lerp.h"
#include "matrix/hub75.h"

typedef enum : uint16_t
{
    None,
    Random,
    Lerp,
} particle_direction_t;

typedef struct
{
    particle_direction_t direction;
    double fade_start;
    double fade_end;
    double move_start;
    double move_end;
    point_t move_from;
    point_t move_to;
    uint8_t col_start;
    uint8_t col_end;
} particle_t;

typedef struct
{
    double start_time;
    particle_t particle;
    bool active;
    double next_move;
    point_t current_position;
} particle_container_t;

#define NUM_PARTICLES 128

class Particles
{
public:
    Particles(Hub75 *hub75)
    {
        _hub75 = hub75;
    }

    void init()
    {
        for (int i = 0; i < NUM_PARTICLES; i++)
        {
            _particles[i] = {};
        }
    }

    void update(double time)
    {
        for (int i = 0; i < NUM_PARTICLES; i++)
        {
            if (_particles[i].active)
            {
                if (_particles[i].start_time < 0)
                {
                    _particles[i].start_time = time;
                }

                if (!animate(time, i))
                {
                    _particles[i].active = false;
                }
            }
        }
    }

    void start_effect(particle_t *particle)
    {
        int i = get_available();

        if (i < 0)
        {
            return;
        }

        memcpy(&_particles[i].particle, particle, sizeof(particle_t));

        _particles[i].next_move = 0;
        _particles[i].current_position.x = particle->move_from.x;
        _particles[i].current_position.y = particle->move_from.y;
        _particles[i].start_time = -1;
        _particles[i].active = true;
    }

private:
    Hub75 *_hub75;
    particle_container_t _particles[NUM_PARTICLES];

    int get_available()
    {
        for (int i = 0; i < NUM_PARTICLES; i++)
        {
            if (!_particles[i].active)
            {
                return i;
            }
        }

        return -1;
    }

    bool animate(double time, int particle)
    {
        particle_t *p = &_particles[particle].particle;

        double offset_time = time - _particles[particle].start_time;

        if ((offset_time > p->fade_end) && (offset_time > p->move_end))
        {
            return false;
        }

        double anim_start = (p->fade_start > p->move_start) ? p->move_start : p->fade_start;

        point_t pt = p->move_from;
        uint8_t c = p->col_start;

        if (p->fade_end > 0)
        {
            if (offset_time > p->fade_start)
            {
                double end = p->fade_end;
                double start = p->fade_start;
                double offset = (offset_time > end) ? end : offset_time;

                uint8_t c1 = p->col_start;
                uint8_t c2 = p->col_end;

                float f = (float)((offset - start) / (end - start));

                c = color_lerp(c1, c2, f);
            }
            else if (offset_time < anim_start)
            {
                return true;
            }
        }

        if ((offset_time > p->move_start) && (p->move_end > 0))
        {
            double end = p->move_end;
            double start = p->move_start;
            double offset = (offset_time > end) ? end : offset_time;
            float f = (float)((offset - start) / (end - start));

            switch (p->direction)
            {
            case None:
                break;
            case Random:
                if (offset_time >= _particles[particle].next_move)
                {
                    pt.x += (rand() % 3) - 1;
                    pt.y += (rand() % 3) - 1;

                    _particles[particle].next_move = offset_time + 100;
                }
                break;
            case Lerp:
                pt = point_lerp(p->move_from, p->move_to, f);
                break;
            }
        }

        _hub75->draw_pixel(pt.x, pt.y, c);

        return true;
    };
};

#endif

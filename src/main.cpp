#include <SFML/Graphics.hpp>

int main()
{
    const float windowWidth = 800.f;
    const float windowHeight = 600.f;
    const float playerSize = 40.f;

    sf::RenderWindow window(
        sf::VideoMode({800, 600}),
        "SHIFT - Two Realities"
    );
    window.setKeyRepeatEnabled(false);  // one Shift press = one swap

    // Middle platform (exactly centered, so the two worlds mirror each other)
    const float midY = windowHeight / 2.f;
    const float midThickness = 20.f;
    const float platformTop = midY - midThickness / 2.f;     // 290
    const float platformBottom = midY + midThickness / 2.f;  // 310

    sf::RectangleShape middle({windowWidth, midThickness});
    middle.setPosition({0.f, platformTop});
    middle.setFillColor(sf::Color(200, 200, 200));

    // Backgrounds for the two worlds
    sf::RectangleShape topBackground({windowWidth, platformTop});
    topBackground.setPosition({0.f, 0.f});
    topBackground.setFillColor(sf::Color(135, 190, 235));    // bright sky

    sf::RectangleShape bottomBackground({windowWidth, windowHeight - platformBottom});
    bottomBackground.setPosition({0.f, platformBottom});
    bottomBackground.setFillColor(sf::Color(40, 10, 30));    // nightmare

    // Player (starts standing on top of the middle platform)
    sf::CircleShape player(playerSize / 2.f);
    player.setFillColor(sf::Color::Green);
    player.setPosition({100.f, platformTop - playerSize});

    bool inTopWorld = true;

    // Physics
    sf::Vector2f velocity{0.f, 0.f};
    const float speed = 300.f;
    const float gravity = 1500.f;
    const float jumpSpeed = -650.f;  // away from the platform in the top world
    bool onGround = false;

    sf::Clock clock;

    while (window.isOpen())
    {
        float dt = clock.restart().asSeconds();

        // +1 in the top world (gravity down), -1 in the bottom world (gravity up)
        float dir = inTopWorld ? 1.f : -1.f;

        // Events
        while (const auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }

            if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>())
            {
                if (keyPressed->code == sf::Keyboard::Key::LShift ||
                    keyPressed->code == sf::Keyboard::Key::RShift)
                {
                    // Switch worlds: mirror the player across the middle line
                    inTopWorld = !inTopWorld;
                    sf::Vector2f p = player.getPosition();
                    player.setPosition({p.x, windowHeight - p.y - playerSize});
                    velocity.y = -velocity.y;
                    dir = inTopWorld ? 1.f : -1.f;
                }
            }
        }

        // Horizontal input
        float moveX = 0.f;

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) ||
            sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))
            moveX -= 1.f;

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) ||
            sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right))
            moveX += 1.f;

        // Jump input (jump direction flips with gravity)
        bool jumpPressed =
            sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space) ||
            sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W) ||
            sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up);

        if (jumpPressed && onGround)
        {
            velocity.y = jumpSpeed * dir;
            onGround = false;
        }

        // Update velocity and position
        velocity.x = moveX * speed;
        velocity.y += gravity * dir * dt;
        player.move(velocity * dt);

        // Screen boundaries
        sf::Vector2f pos = player.getPosition();

        if (pos.x < 0.f)
            pos.x = 0.f;
        if (pos.x > windowWidth - playerSize)
            pos.x = windowWidth - playerSize;

        if (pos.y < 0.f)
        {
            pos.y = 0.f;
            velocity.y = 0.f;
        }
        if (pos.y > windowHeight - playerSize)
        {
            pos.y = windowHeight - playerSize;
            velocity.y = 0.f;
        }

        // Middle platform collision
        onGround = false;

        if (inTopWorld)
        {
            // Stand on top of the platform
            if (pos.y + playerSize > platformTop)
            {
                pos.y = platformTop - playerSize;
                velocity.y = 0.f;
                onGround = true;
            }
        }
        else
        {
            // Hang from the underside of the platform
            if (pos.y < platformBottom)
            {
                pos.y = platformBottom;
                velocity.y = 0.f;
                onGround = true;
            }
        }

        player.setPosition(pos);

        // Draw
        window.clear(sf::Color::Black);
        window.draw(topBackground);
        window.draw(bottomBackground);
        window.draw(middle);
        window.draw(player);
        window.display();
    }

    return 0;
}
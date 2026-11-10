#include <SFML/Graphics.hpp>
#include <vector>

int main()
{
    const float windowWidth = 800.f;
    const float playerSize = 60.f;  // circle radius 30 -> 60 px wide and tall

    sf::RenderWindow window(
        sf::VideoMode({800, 600}),
        "SHIFT - Two Realities"
    );

    // Player
    sf::CircleShape player(playerSize / 2.f);
    player.setFillColor(sf::Color::Green);
    player.setPosition({385.f, 285.f});

    // Ground
    const float groundY = 540.f;
    sf::RectangleShape ground({windowWidth, 60.f});
    ground.setPosition({0.f, groundY});
    ground.setFillColor(sf::Color(90, 90, 90));

    // Platforms
    std::vector<sf::RectangleShape> platforms;

    sf::RectangleShape platform1({200.f, 20.f});
    platform1.setPosition({100.f, 430.f});
    platform1.setFillColor(sf::Color(150, 100, 50));
    platforms.push_back(platform1);

    sf::RectangleShape platform2({200.f, 20.f});
    platform2.setPosition({450.f, 330.f});
    platform2.setFillColor(sf::Color(150, 100, 50));
    platforms.push_back(platform2);

    // Physics
    sf::Vector2f velocity{0.f, 0.f};
    const float speed = 300.f;
    const float gravity = 1500.f;
    const float jumpSpeed = -650.f;  // negative = upward
    bool onGround = false;

    sf::Clock clock;

    while (window.isOpen())
    {
        float dt = clock.restart().asSeconds();

        // Events
        while (const auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
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

        // Jump input
        bool jumpPressed =
            sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space) ||
            sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W) ||
            sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up);

        if (jumpPressed && onGround)
        {
            velocity.y = jumpSpeed;
            onGround = false;
        }

        // Remember where the player's feet were before moving
        float prevBottom = player.getPosition().y + playerSize;

        // Update velocity and position
        velocity.x = moveX * speed;
        velocity.y += gravity * dt;
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

        player.setPosition(pos);

        // Ground collision
        onGround = false;
        float bottom = player.getPosition().y + playerSize;
        if (bottom > groundY)
        {
            player.setPosition({player.getPosition().x, groundY - playerSize});
            velocity.y = 0.f;
            onGround = true;
        }

        // Platform collision (landing from above)
        for (const auto& platform : platforms)
        {
            sf::Vector2f playerPos = player.getPosition();
            sf::Vector2f platPos = platform.getPosition();
            sf::Vector2f platSize = platform.getSize();

            float playerBottom = playerPos.y + playerSize;

            bool overlapX = playerPos.x + playerSize > platPos.x &&
                            playerPos.x < platPos.x + platSize.x;

            bool wasAbove = prevBottom <= platPos.y;
            bool nowBelowTop = playerBottom >= platPos.y;

            if (velocity.y >= 0.f && overlapX && wasAbove && nowBelowTop)
            {
                player.setPosition({playerPos.x, platPos.y - playerSize});
                velocity.y = 0.f;
                onGround = true;
            }
        }

        // Draw
        window.clear(sf::Color::Black);
        window.draw(ground);
        for (const auto& platform : platforms)
            window.draw(platform);
        window.draw(player);
        window.display();
    }

    return 0;
}
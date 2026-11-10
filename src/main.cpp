#include <SFML/Graphics.hpp>
#include <vector>

struct Obstacle
{
    sf::RectangleShape shape;
    bool inTopWorld;
    bool deadly;
};

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

    // Middle platform (centered, so the two worlds mirror each other)
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

    // Player
    const sf::Vector2f startPos{60.f, platformTop - playerSize};
    sf::CircleShape player(playerSize / 2.f);
    player.setFillColor(sf::Color::Green);
    player.setPosition(startPos);

    bool inTopWorld = true;

    // Physics
    sf::Vector2f velocity{0.f, 0.f};
    const float speed = 300.f;
    const float gravity = 1500.f;
    const float jumpSpeed = -650.f;  // away from the platform in the top world
    bool onGround = false;

    // Helper: do the player's square and a rectangle overlap?
    auto overlaps = [](sf::Vector2f p, float size, const sf::RectangleShape& r)
    {
        sf::Vector2f rp = r.getPosition();
        sf::Vector2f rs = r.getSize();
        return p.x < rp.x + rs.x && p.x + size > rp.x &&
               p.y < rp.y + rs.y && p.y + size > rp.y;
    };

    // Helper: send the player back to the start
    auto restart = [&]()
    {
        player.setPosition(startPos);
        velocity = {0.f, 0.f};
        inTopWorld = true;
        onGround = false;
    };

    // Obstacles
    std::vector<Obstacle> obstacles;

    // addObstacle(inTopWorld, deadly, x, width, height)
    auto addObstacle = [&](bool topWorld, bool deadly, float x, float width, float height)
    {
        Obstacle o{sf::RectangleShape({width, height}), topWorld, deadly};
        float y = topWorld ? platformTop - height : platformBottom;
        o.shape.setPosition({x, y});

        if (deadly)
            o.shape.setFillColor(sf::Color(200, 40, 40));
        else if (topWorld)
            o.shape.setFillColor(sf::Color(120, 90, 60));
        else
            o.shape.setFillColor(sf::Color(110, 60, 120));

        obstacles.push_back(o);
    };

    // Test layout
    addObstacle(true,  false, 140.f,  60.f, 60.f);   // top: solid step
    addObstacle(true,  true,  250.f, 300.f, 30.f);   // top: wide deadly strip
    addObstacle(false, false, 380.f,  60.f, 60.f);   // bottom: solid block
    addObstacle(false, false, 680.f,  60.f, 60.f);   // bottom: solid block

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
                    // Where would the player appear after the shift?
                    sf::Vector2f p = player.getPosition();
                    sf::Vector2f mirrored{p.x, windowHeight - p.y - playerSize};
                    bool targetTop = !inTopWorld;

                    // Blocked if a solid obstacle already occupies that spot
                    bool blocked = false;
                    for (const auto& o : obstacles)
                    {
                        if (o.inTopWorld == targetTop && !o.deadly &&
                            overlaps(mirrored, playerSize, o.shape))
                        {
                            blocked = true;
                        }
                    }

                    if (!blocked)
                    {
                        inTopWorld = targetTop;
                        player.setPosition(mirrored);
                        velocity.y = -velocity.y;
                        dir = inTopWorld ? 1.f : -1.f;
                    }
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

        velocity.x = moveX * speed;
        velocity.y += gravity * dir * dt;

        sf::Vector2f pos = player.getPosition();

        // ---- 1) Move horizontally, then push out of solid obstacles ----
        pos.x += velocity.x * dt;

        if (pos.x < 0.f)
            pos.x = 0.f;
        if (pos.x > windowWidth - playerSize)
            pos.x = windowWidth - playerSize;

        for (const auto& o : obstacles)
        {
            if (o.inTopWorld != inTopWorld || o.deadly)
                continue;
            if (!overlaps(pos, playerSize, o.shape))
                continue;

            sf::Vector2f op = o.shape.getPosition();
            sf::Vector2f os = o.shape.getSize();

            if (velocity.x > 0.f)
                pos.x = op.x - playerSize;   // hit its left side
            else if (velocity.x < 0.f)
                pos.x = op.x + os.x;         // hit its right side
        }

        // ---- 2) Move vertically, then push out of solid obstacles ----
        pos.y += velocity.y * dt;

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

        onGround = false;

        for (const auto& o : obstacles)
        {
            if (o.inTopWorld != inTopWorld || o.deadly)
                continue;
            if (!overlaps(pos, playerSize, o.shape))
                continue;

            sf::Vector2f op = o.shape.getPosition();
            sf::Vector2f os = o.shape.getSize();

            bool movingTowardGround = (velocity.y * dir > 0.f);

            if (movingTowardGround)
            {
                // Land on the obstacle's surface
                pos.y = inTopWorld ? op.y - playerSize : op.y + os.y;
                onGround = true;
            }
            else
            {
                // Bump into the obstacle's other side
                pos.y = inTopWorld ? op.y + os.y : op.y - playerSize;
            }
            velocity.y = 0.f;
        }

        // Middle platform collision
        if (inTopWorld)
        {
            if (pos.y + playerSize > platformTop)
            {
                pos.y = platformTop - playerSize;
                velocity.y = 0.f;
                onGround = true;
            }
        }
        else
        {
            if (pos.y < platformBottom)
            {
                pos.y = platformBottom;
                velocity.y = 0.f;
                onGround = true;
            }
        }

        player.setPosition(pos);

        // ---- 3) Deadly obstacles (slightly forgiving hitbox) ----
        for (const auto& o : obstacles)
        {
            if (o.inTopWorld != inTopWorld || !o.deadly)
                continue;

            if (overlaps({pos.x + 4.f, pos.y + 4.f}, playerSize - 8.f, o.shape))
            {
                restart();
                break;
            }
        }

        // Draw
        window.clear(sf::Color::Black);
        window.draw(topBackground);
        window.draw(bottomBackground);
        window.draw(middle);

        for (const auto& o : obstacles)
            window.draw(o.shape);

        window.draw(player);
        window.display();
    }

    return 0;
}
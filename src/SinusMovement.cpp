#include "SinusMovement.hpp"

#include "DEngine/SceneObject.hpp"
#include "DEngine/Scene.hpp"

#include "DMath/Trigonometric.hpp"

#include <iostream>

namespace Assignment02
{
	SinusMovement::SinusMovement(Engine::SceneObject& owner) :
		ParentType(owner)
	{

	}

	void SinusMovement::SceneStart()
	{
		ParentType::SceneStart();

		startPos = GetSceneObject().localPosition;
	}

	void SinusMovement::Tick()
	{
		ParentType::Tick();

		const Engine::Scene& scene = GetSceneObject().GetScene();

		float time = scene.GetTimeData().GetTimeSinceSceneStart();

		float timeMultiplier = 25.f;
		float moveMagnitudeMultipler = 7.5f;
		GetSceneObject().localPosition.x = startPos.x + Math::Sin(time * timeMultiplier) * moveMagnitudeMultipler;
	}
}
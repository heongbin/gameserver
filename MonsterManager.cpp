#include "pch.h"
#include "MonsterManager.h"
#include "TimerManager.h"
#include "Timer.h"
#include "Monster.h"
#include "GameSession.h"
#include "GameSessionManager.h"
#include "BufferWriter.h"
#include "Ability.h"
#include "Worker.h"


MonsterManager GMonsterManager;

MonsterManager::MonsterManager()
{
}

MonsterManager::~MonsterManager()
{
}

void MonsterManager::Add(shared_ptr<Monster> monster)
{
	monster->MonsterId = SetId();
	Monsters.insert(make_pair(monster->MonsterId, monster));
}

void MonsterManager::Delete(uint16 id)
{
	Monsters.erase(id);
}

shared_ptr<Monster> MonsterManager::GetMonsterFromId(uint16 id)
{
	if (Monsters.find(id) != Monsters.end())
		return static_pointer_cast<Monster>(Monsters[id]->shared_from_this());
	return nullptr;
}

map<uint16, shared_ptr<Monster>>& MonsterManager::GetMonsterMap()
{
	 return Monsters; 
}

bool MonsterManager::MonsterDamaged(Vector<Monster>& hitmonsters,GameSession* _session,float _x, float _y, float forward_x, float forward_y)
{
	
	bool hitSuccess = false;
	for (auto& monster : GMonsterManager.GetMonsterMap())
	{
		gWorker->DoAsync([&]() {
			if (_session->playerinfo->IsMonsterInAttackRange(monster.second, _x, _y, forward_x, forward_y))
			{
				hitSuccess = true;
				monster.second->Hp -= _session->playerinfo->Attack;
				cout << "Player " << _session->id << " attack to monster " << monster.second->MonsterId << endl;
				hitmonsters.push_back(*monster.second);
				if (monster.second->Hp <= 0)
				{

				}
			}
			}
		);
	}
	return hitSuccess;
}

void MonsterManager::Manage_Monsters()
{
	Timer* timer = xnew<Timer>(Timer());
	timer->Reset();
	float elapsedtime = 0.f;
	Vector<uint16> gone_ids;
	while (true)
	{
		//일부로직 제거 코드보여주기 용이라.
		//15프레임
		if ()
		{
			//이부분도 일부 로직 제거 코드보여주기 용일뿐이라.

			gWorker->DoAsync(
				[=,&elapsedtime]()
				{
					//WriteLockGuard Wlock(GTimerManger->GetLock(), typeid(this).name());

					//GPhysx->getScene().lockWrite();
					//WriteLockGuard Wlock(GTimerManger->GetLock(), typeid(this).name());
					{
						//WRITE_LOCK;
						manage_stunned_monster();
						for (auto m : GMonsterManager.GetMonsterMap())
						{
							if (m.second == nullptr) continue;
							if (m.second->MonsterState == CommonState::ATTACKING)
							{
								if (m.second->SkillTimer->GetIsRunning() == false)
								{
									m.second->SkillTimer->SetIsRunning(true);
									m.second->SkillTimer->Reset();
								}
								else
								{
									m.second->SkillTimer->Tick();
									if (m.second->SkillCooldown <= m.second->SkillTimer->DeltaTime())
									{
										m.second->bSkillReady = true;
										m.second->SkillTimer->SetIsRunning(false);
									}

								}

							}
							else if (m.second->MonsterState == CommonState::GONE) {
								gone_ids.push_back(m.second->MonsterId);
							}
							else if (m.second->MonsterState == CommonState::CASTING || m.second->MonsterState == CommonState::STUN || m.second->MonsterState == CommonState::IDLE)
							{
								m.second->VX = 0.f;
								m.second->VY = 0.f;
								m.second->VZ = 0.f;

							}
							else if (m.second->MonsterState == CommonState::TRACKING)
							{
								m.second->MoveTo(m.second->target.lock());
							}
							else if (m.second->MonsterState == CommonState::ATTACKING)
							{
								m.second->VX = 0.f;
								m.second->VY = 0.f;
								m.second->VZ = 0.f;
							}
							else if (m.second->MonsterState == CommonState::ATTACKINGREADY)
							{
								m.second->VX = 0.f;
								m.second->VY = 0.f;
								m.second->VZ = 0.f;
								if (m.second->bSkillReady)
								{
									m.second->ActivateSkill();

									continue;
								}

								m.second->Attack(m.second->target.lock());
							}
							//else if(m.second->monsterState==CommonState::CASTRING)
						}
					}
					if (!gone_ids.empty())
					{
						for (auto& id : gone_ids)
						{
							//WRITE_LOCK;
							if (GMonsterManager.GetMonsterFromId(id)->meleeAttackTimer != nullptr)
							{
								GMonsterManager.GetMonsterFromId(id)->meleeAttackTimer->SetIsRunning(false);
								GMonsterManager.GetMonsterFromId(id)->meleeAttackTimer->call_back_ptr = nullptr;
								GMonsterManager.GetMonsterFromId(id)->meleeAttackTimer = nullptr;
							}

							if (GMonsterManager.GetMonsterFromId(id)->SkillTimer != nullptr)
							{
								GMonsterManager.GetMonsterFromId(id)->SkillTimer->call_back_ptr = nullptr;
								GMonsterManager.GetMonsterFromId(id)->SkillTimer->SetIsRunning(false);
								GMonsterManager.GetMonsterFromId(id)->SkillTimer = nullptr;
							}
							GMonsterManager.GetMonsterFromId(id)->mMonsterCollision->GetPhysxActor()->userData = nullptr;
							GMonsterManager.Delete(id);
						}

						gone_ids.clear();
					}

					// 0.33초마다 클라이언트에게 몬스터 정보 전송
					//일부로직제거 코드 보여주기용이라
						header->size = bw.WriteSize();
						sendBuffer->Close(bw.WriteSize());
						GSessionManager.Broadcast(sendBuffer);
					}


				}
				}
			);
		}
			
	}
}

void MonsterManager::Monster_attack(uint16 monsterid)
{
	WRITE_LOCK;
	if (Monsters.find(monsterid) == Monsters.end())return;
	if (Monsters[monsterid]->MonsterState == CommonState::STUN || Monsters[monsterid]->MonsterState == CommonState::TRACKING)
	{
		return;
	}
	Monsters[monsterid]->MonsterState = CommonState::ATTACKINGREADY;

}

void MonsterManager::Monster_Overlap_Detection(const physx::PxContactPair& cp, Monster* monster, Player* target)
{
	if (cp.events & physx::PxPairFlag::eNOTIFY_TOUCH_FOUND)
	{
		if (cp.shapes[0] == monster->mMonsterCollision->GetOgreStrikeCollision() || cp.shapes[1] == monster->mMonsterCollision->GetOgreStrikeCollision())
		{
			Ogre_Ground_Strike* skill = static_cast<Ogre_Ground_Strike*>(monster->skills[0]);
			auto tmp = find(skill->victims.begin(), skill->victims.end(), target);
			if (tmp == skill->victims.end()) {
				Ogre_Ground_Strike* skill = static_cast<Ogre_Ground_Strike*>(monster->skills[0]);
				skill->victims.push_back(target);
			}
		}
	}
	else if (cp.events & physx::PxPairFlag::eNOTIFY_TOUCH_PERSISTS)
	{
		if (cp.shapes[0] == monster->mMonsterCollision->GetMeleeCollision() || cp.shapes[1] == monster->mMonsterCollision->GetMeleeCollision())
		{
			if (monster->target.lock() == nullptr && target!=nullptr)monster->target = static_pointer_cast<Player>(target->shared_from_this());
			if (monster->meleeAttackTimer != nullptr) return;
			if (monster->MonsterState == CommonState::STUN || monster->MonsterState == CommonState::ATTACKING || monster->MonsterState == CommonState::CASTING)return;
			monster->MonsterState = CommonState::ATTACKINGREADY;
			
		}
		else if (cp.shapes[0] == monster->mMonsterCollision->GetTraceCollision() || cp.shapes[1] == monster->mMonsterCollision->GetTraceCollision())
		{
			if (monster->MonsterState == CommonState::STUN || monster->MonsterState == CommonState::CASTING)return;
			if (monster->target.lock() == nullptr && target!=nullptr)
			{
				monster->target = static_pointer_cast<Player>(target->shared_from_this());
				monster->MonsterState = CommonState::TRACKING;
			}
			else if (monster->target.lock().get()== target && monster->MonsterState == CommonState::IDLE)
			{
				monster->MonsterState = CommonState::TRACKING;
			}
		}


	}
	else if (cp.events & physx::PxPairFlag::eNOTIFY_TOUCH_LOST)
	{
		if (cp.shapes[0] == monster->mMonsterCollision->GetTraceCollision() || cp.shapes[1] == monster->mMonsterCollision->GetTraceCollision())
		{
			if (monster->MonsterState == CommonState::CASTING)
			{
				monster->target.reset();
				return;
			}
			if (target == monster->target.lock().get())
			{
				monster->MonsterState = CommonState::IDLE;
				monster->target.reset();
			}

		}
		else if (cp.shapes[0] == monster->mMonsterCollision->GetMeleeCollision() || cp.shapes[1] == monster->mMonsterCollision->GetMeleeCollision())
		{
			if (monster->MonsterState == CommonState::CASTING)
			{
				return;
			}
			if (target == monster->target.lock().get())
			{
				monster->MonsterState = CommonState::TRACKING;
			}
		}
		
		else if (cp.shapes[0] == monster->mMonsterCollision->GetOgreStrikeCollision() || cp.shapes[1] == monster->mMonsterCollision->GetOgreStrikeCollision())
		{
			Ogre_Ground_Strike* skill = static_cast<Ogre_Ground_Strike*>(monster->skills[0]);
			auto tmp = find(skill->victims.begin(), skill->victims.end(), target);
			if (tmp != skill->victims.end())
			{
				skill->victims.erase(tmp);
			}
		}
		
	}
}

void MonsterManager::insert_queue(std::shared_ptr<Monster> monster)
{
	//WriteLockGuard Wlock(GTimerManger->lock(), typeid(this).name());
	WRITE_LOCK;
	stunned_monsters.push(monster);
}

void MonsterManager::manage_stunned_monster()
{
	while (!stunned_monsters.empty())
	{
		std::shared_ptr<Monster> monster = stunned_monsters.front();
		stunned_monsters.pop();
		if (monster == nullptr)continue;
		monster->MonsterState = CommonState::STUN;

		GTimerManger->SetTimer(monster.get(), 3.f, [=]() {
			WRITE_LOCK;
			monster->MonsterState = CommonState::IDLE;
			});
	}
}

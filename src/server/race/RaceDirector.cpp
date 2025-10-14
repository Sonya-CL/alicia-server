/**
 * Alicia Server - dedicated server software
 * Copyright (C) 2024 Story Of Alicia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **/

#include "server/race/RaceDirector.hpp"

#include "libserver/data/helper/ProtocolHelper.hpp"
#include "server/ServerInstance.hpp"

#include "server/system/RoomSystem.hpp"

#include <spdlog/spdlog.h>
#include <bitset>
#include <random>

namespace server
{

namespace
{

//! Converts a steady clock's time point to a race clock's time point.
//! @param timePoint Time point.
//! @return Race clock time point.
uint64_t TimePointToRaceTimePoint(const std::chrono::steady_clock::time_point& timePoint)
{
  // Amount of 100ns
  constexpr uint64_t IntervalConstant = 100;
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
    timePoint.time_since_epoch()).count() / IntervalConstant;
}

} // anon namespace

RaceDirector::RaceDirector(ServerInstance& serverInstance)
  : _serverInstance(serverInstance)
  , _commandServer(*this)
{
  _commandServer.RegisterCommandHandler<protocol::AcCmdCREnterRoom>(
    [this](ClientId clientId, const auto& message)
    {
      HandleEnterRoom(clientId, message);
    });

  _commandServer.RegisterCommandHandler<protocol::AcCmdCRChangeRoomOptions>(
    [this](ClientId clientId, const auto& message)
    {
      HandleChangeRoomOptions(clientId, message);
    });

  _commandServer.RegisterCommandHandler<protocol::AcCmdCRChangeTeam>(
    [this](ClientId clientId, const auto& message)
    {
      HandleChangeTeam(clientId, message);
    });

  _commandServer.RegisterCommandHandler<protocol::AcCmdCRLeaveRoom>(
    [this](ClientId clientId, const auto& message)
    {
      HandleLeaveRoom(clientId);
    });

  _commandServer.RegisterCommandHandler<protocol::AcCmdCRStartRace>(
    [this](ClientId clientId, const auto& message)
    {
      HandleStartRace(clientId, message);
    });

  _commandServer.RegisterCommandHandler<protocol::AcCmdUserRaceTimer>(
    [this](ClientId clientId, const auto& message)
    {
      HandleRaceTimer(clientId, message);
    });

  _commandServer.RegisterCommandHandler<protocol::AcCmdCRLoadingComplete>(
    [this](ClientId clientId, const auto& message)
    {
      HandleLoadingComplete(clientId, message);
    });

  _commandServer.RegisterCommandHandler<protocol::AcCmdCRReadyRace>(
    [this](ClientId clientId, const auto& message)
    {
      HandleReadyRace(clientId, message);
    });

  _commandServer.RegisterCommandHandler<protocol::AcCmdUserRaceFinal>(
    [this](ClientId clientId, const auto& message)
    {
      HandleUserRaceFinal(clientId, message);
    });

  _commandServer.RegisterCommandHandler<protocol::AcCmdCRRaceResult>(
    [this](ClientId clientId, const auto& message)
    {
      HandleRaceResult(clientId, message);
    });

  _commandServer.RegisterCommandHandler<protocol::AcCmdCRP2PResult>(
    [this](ClientId clientId, const auto& message)
    {
      HandleP2PRaceResult(clientId, message);
    });

  _commandServer.RegisterCommandHandler<protocol::AcCmdUserRaceP2PResult>(
    [this](ClientId clientId, const auto& message)
    {
      HandleP2PUserRaceResult(clientId, message);
    });

  _commandServer.RegisterCommandHandler<protocol::AcCmdCRAwardStart>(
    [this](ClientId clientId, const auto& message)
    {
      HandleAwardStart(clientId, message);
    });

  _commandServer.RegisterCommandHandler<protocol::AcCmdCRAwardEnd>(
    [this](ClientId clientId, const auto& message)
    {
      HandleAwardEnd(clientId, message);
    });

  _commandServer.RegisterCommandHandler<protocol::AcCmdCRStarPointGet>(
    [this](ClientId clientId, const auto& message)
    {
      HandleStarPointGet(clientId, message);
    });

  _commandServer.RegisterCommandHandler<protocol::AcCmdCRRequestSpur>(
    [this](ClientId clientId, const auto& message)
    {
      HandleRequestSpur(clientId, message);
    });

  _commandServer.RegisterCommandHandler<protocol::AcCmdCRHurdleClearResult>(
    [this](ClientId clientId, const auto& message)
    {
      HandleHurdleClearResult(clientId, message);
    });

  _commandServer.RegisterCommandHandler<protocol::AcCmdCRStartingRate>(
    [this](ClientId clientId, const auto& message)
    {
      HandleStartingRate(clientId, message);
    });

  _commandServer.RegisterCommandHandler<protocol::AcCmdUserRaceUpdatePos>(
    [this](ClientId clientId, const auto& message)
    {
      HandleRaceUserPos(clientId, message);
    });

  _commandServer.RegisterCommandHandler<protocol::AcCmdCRChat>(
    [this](ClientId clientId, const auto& message)
    {
      HandleChat(clientId, message);
    });

  _commandServer.RegisterCommandHandler<protocol::AcCmdCRRelayCommand>(
    [this](ClientId clientId, const auto& message)
    {
      HandleRelayCommand(clientId, message);
    });

  _commandServer.RegisterCommandHandler<protocol::AcCmdCRRelay>(
    [this](ClientId clientId, const auto& message)
    {
      HandleRelay(clientId, message);
    });

  _commandServer.RegisterCommandHandler<protocol::AcCmdUserRaceActivateInteractiveEvent>(
    [this](ClientId clientId, const auto& message)
    {
      HandleUserRaceActivateInteractiveEvent(clientId, message);
    });

  _commandServer.RegisterCommandHandler<protocol::AcCmdUserRaceActivateEvent>(
    [this](ClientId clientId, const auto& message)
     {
       HandleUserRaceActivateEvent(clientId, message);
     });

  _commandServer.RegisterCommandHandler<protocol::AcCmdCRRequestMagicItem>(
    [this](ClientId clientId, const auto& message)
    {
      HandleRequestMagicItem(clientId, message);
    });

  _commandServer.RegisterCommandHandler<protocol::AcCmdCRUseMagicItem>(
    [this](ClientId clientId, const auto& message)
    {
      HandleUseMagicItem(clientId, message);
    });

  _commandServer.RegisterCommandHandler<protocol::AcCmdUserRaceItemGet>(
    [this](ClientId clientId, const auto& message)
    {
      HandleUserRaceItemGet(clientId, message);
    });

  // Magic Targeting Commands for Bolt System
  _commandServer.RegisterCommandHandler<protocol::AcCmdCRStartMagicTarget>(
    [this](ClientId clientId, const auto& message)
    {
      HandleStartMagicTarget(clientId, message);
    });

  _commandServer.RegisterCommandHandler<protocol::AcCmdCRChangeMagicTargetNotify>(
    [this](ClientId clientId, const auto& message)
    {
      HandleChangeMagicTargetNotify(clientId, message);
    });

  _commandServer.RegisterCommandHandler<protocol::AcCmdCRChangeMagicTargetOK>(
    [this](ClientId clientId, const auto& message)
    {
      HandleChangeMagicTargetOK(clientId, message);
    });

  _commandServer.RegisterCommandHandler<protocol::AcCmdCRChangeMagicTargetCancel>(
    [this](ClientId clientId, const auto& message)
    {
      HandleChangeMagicTargetCancel(clientId, message);
    });

  _commandServer.RegisterCommandHandler<protocol::AcCmdCRTriggerEvent>(
    [this](ClientId clientId, const auto& message)
    {
      HandleTriggerEvent(clientId, message);
    });
  
  _commandServer.RegisterCommandHandler<protocol::AcCmdCRActivateSkillEffect>(
    [this](ClientId clientId, const auto& message)
    {
      HandleActivateSkillEffect(clientId, message);
    });
}

void RaceDirector::Initialize()
{
  spdlog::debug(
    "Race server listening on {}:{}",
    GetConfig().listen.address.to_string(),
    GetConfig().listen.port);

  test = std::thread([this]()
  {
    std::unordered_set<asio::ip::udp::endpoint> _clients;

    asio::io_context ioCtx;
    asio::ip::udp::socket skt(
      ioCtx,
      asio::ip::udp::endpoint(
        asio::ip::address_v4::loopback(),
        10500));

    asio::streambuf readBuffer;
    asio::streambuf writeBuffer;

    while (run_test)
    {
      const auto request = readBuffer.prepare(1024);
      asio::ip::udp::endpoint sender;

      try
      {
        const size_t bytesRead = skt.receive_from(request, sender);

        struct RelayHeader
        {
          uint16_t member0{};
          uint16_t member1{};
          uint16_t member2{};
        };

        const auto response = writeBuffer.prepare(1024);

        RelayHeader* header = static_cast<RelayHeader*>(response.data());
        header->member2 = 1;

        for (const auto idx : std::views::iota(0ull, bytesRead))
        {
          static_cast<std::byte*>(response.data())[idx + sizeof(RelayHeader)] = static_cast<const std::byte*>(
            request.data())[idx];
        }

        writeBuffer.commit(bytesRead + sizeof(RelayHeader));
        readBuffer.consume(bytesRead + sizeof(RelayHeader));

        for (auto& client : _clients)
        {
          if (client == sender)
            continue;

          writeBuffer.consume(
            skt.send_to(writeBuffer.data(), client));
        }

        if (not _clients.contains(sender))
          _clients.insert(sender);

      } catch (const std::exception& x) {
      }

    }
  });
  test.detach();

  _commandServer.BeginHost(GetConfig().listen.address, GetConfig().listen.port);
}

void RaceDirector::Terminate()
{
  run_test = false;
  _commandServer.EndHost();
}

void RaceDirector::Tick() {
  _scheduler.Tick();

  // Process rooms which are waiting
  for (auto& [roomUid, roomInstance] : _roomInstances)
  {
    if (roomInstance.stage != RoomInstance::Stage::Waiting)
      continue;

    // todo: room countdown handling
  }

  // Process rooms which are loading
  for (auto& roomInstance : _roomInstances | std::views::values)
  {
    if (roomInstance.stage != RoomInstance::Stage::Loading)
      continue;

    const bool allRacersLoaded = std::ranges::all_of(
      std::views::values(roomInstance.tracker.GetRacers()),
      [](const tracker::RaceTracker::Racer& racer)
      {
        return racer.state == tracker::RaceTracker::Racer::State::Racing;
      });

    const bool loadTimeoutReached = std::chrono::steady_clock::now() >= roomInstance.stageTimeoutTimePoint;

    // If not all of the racer have loaded yet and the timeout has not been reached yet
    // do not start the race.
    if (not allRacersLoaded && not loadTimeoutReached)
      continue;

    // TODO: better way of doing this? Reinstantiating the room?
    // Clear room items before populating
    roomInstance.tracker.GetItems().clear();

    const auto mapBlockTemplate = _serverInstance.GetCourseRegistry().GetMapBlockInfo(
      roomInstance.mapBlockId);

    // Switch to the racing stage and set the timeout time point.
    roomInstance.stage = RoomInstance::Stage::Racing;
    roomInstance.stageTimeoutTimePoint = std::chrono::steady_clock::now() + std::chrono::seconds(
      mapBlockTemplate.timeLimit);

    // Set up the race start time point.
    const auto now = std::chrono::steady_clock::now();
    roomInstance.raceStartTimePoint = now + std::chrono::seconds(
      mapBlockTemplate.waitTime);

    const protocol::AcCmdUserRaceCountdown raceCountdown{
      .raceStartTimestamp = TimePointToRaceTimePoint(
        roomInstance.raceStartTimePoint)};

    // Broadcast the race countdown.
    for (const ClientId& roomClientId : roomInstance.clients)
    {
      _commandServer.QueueCommand<protocol::AcCmdUserRaceCountdown>(
        roomClientId,
        [raceCountdown]()
        {
          return raceCountdown;
        });
    }
  }

  // Process rooms which are racing
  for (auto& roomInstance : _roomInstances | std::views::values)
  {
    if (roomInstance.stage != RoomInstance::Stage::Racing)
      continue;

    protocol::AcCmdRCRaceResultNotify raceResult{};

    const bool allRacersFinished = std::ranges::all_of(
      std::views::values(roomInstance.tracker.GetRacers()),
      [](const tracker::RaceTracker::Racer& racer)
      {
        return racer.state == tracker::RaceTracker::Racer::State::Finishing;
      });

    const bool raceTimeoutReached = std::chrono::steady_clock::now() >= roomInstance.stageTimeoutTimePoint;

    // If not all of the racer have finished yet and the timeout has not been reached yet
    // do not finish the race.
    if (not allRacersFinished && not raceTimeoutReached)
      return;

    roomInstance.stage = RoomInstance::Stage::Waiting;

    // Build the score board.
    for (auto& [characterUid, racer] : roomInstance.tracker.GetRacers())
    {
      auto& score = raceResult.scores.emplace_back();

      racer.state = tracker::RaceTracker::Racer::State::NotReady;

      // todo: figure out the other bit set values

      if (racer.state != tracker::RaceTracker::Racer::State::Disconnected)
      {
        score.bitset = static_cast<protocol::AcCmdRCRaceResultNotify::ScoreInfo::Bitset>(
            protocol::AcCmdRCRaceResultNotify::ScoreInfo::Bitset::Connected | 0b1'1111);
      }

      score.courseTime = racer.courseTime;

      const auto characterRecord = _serverInstance.GetDataDirector().GetCharacter(
        characterUid);

      characterRecord.Immutable([this, &score](const data::Character& character)
      {
        score.uid = character.uid();
        score.name = character.name();
        score.level = character.level();

        _serverInstance.GetDataDirector().GetHorse(character.mountUid()).Immutable(
          [&score](const data::Horse& horse)
          {
            score.mountName = horse.name();
          });
      });
    }

    // Broadcast the race result
    for (const ClientId roomClientId : roomInstance.clients)
    {
      _commandServer.QueueCommand<decltype(raceResult)>(
        roomClientId,
        [raceResult]()
        {
          return raceResult;
        });
    }
  }
}

void RaceDirector::HandleClientConnected(ClientId clientId)
{
  _clients.try_emplace(clientId);

  spdlog::info("Client {} connected to the race", clientId);
}

void RaceDirector::HandleClientDisconnected(ClientId clientId)
{
  const auto& clientContext = _clients[clientId];

  const auto roomIter = _roomInstances.find(
    clientContext.roomUid);
  if (roomIter != _roomInstances.cend())
  {
    HandleLeaveRoom(clientId);
  }

  spdlog::info("Client {} disconnected from the race", clientId);
  _clients.erase(clientId);
}

ServerInstance& RaceDirector::GetServerInstance()
{
  return _serverInstance;
}

Config::Race& RaceDirector::GetConfig()
{
  return GetServerInstance().GetSettings().race;
}

void RaceDirector::HandleEnterRoom(
  ClientId clientId,
  const protocol::AcCmdCREnterRoom& command)
{
  auto& clientContext = _clients[clientId];
  clientContext.characterUid = command.characterUid;
  clientContext.roomUid = command.roomUid;

  const auto& room = _serverInstance.GetRoomSystem().GetRoom(
    command.roomUid);

  // todo: verify otp

  const auto& [roomInstanceIter, inserted] = _roomInstances.try_emplace(
    command.roomUid);
  auto& roomInstance = roomInstanceIter->second;

  // If the room instance was just created, set it up.
  if (inserted)
  {
    roomInstance.masterUid = command.characterUid;
  }

  _serverInstance.GetDataDirector().GetCharacter(clientContext.characterUid).Immutable(
    [roomUid = clientContext.roomUid, inserted](
      const data::Character& character)
    {
      if (inserted)
        spdlog::info("Character '{}' has created the room {}", character.name(), roomUid);
      else
        spdlog::info("Character '{}' has joined the room {}", character.name(), roomUid);
    });

  auto& joinedRacer = roomInstance.tracker.AddRacer(
    command.characterUid);

  joinedRacer.state = tracker::RaceTracker::Racer::State::NotReady;

  // Todo: Roll the code for the connecting client.
  // Todo: The response contains the code, somewhere.
  _commandServer.SetCode(clientId, {});

  protocol::AcCmdCREnterRoomOK response{
    .nowPlaying = 1,
    .uid = room.uid,
    .roomDescription = {
      .name = room.name,
      .playerCount = room.playerCount,
      .password = room.password,
      .gameModeMaps = room.gameMode,
      .gameMode = room.gameMode,
      .mapBlockId = room.mapBlockId,
      .teamMode = room.teamMode,
      .missionId = room.missionId,
      .unk6 = room.unk3,
      .skillBracket = room.unk4}};

  protocol::Racer joiningRacer;

  for (const auto& [characterUid, racer] : roomInstance.tracker.GetRacers())
  {
    auto& protocolRacer = response.racers.emplace_back();

    protocolRacer.isReady = racer.state == tracker::RaceTracker::Racer::State::Waiting;

    const auto characterRecord = GetServerInstance().GetDataDirector().GetCharacter(
      characterUid);
    characterRecord.Immutable(
      [this, racer, &protocolRacer, leaderUid = roomInstance.masterUid](
        const data::Character& character)
      {
        if (character.uid() == leaderUid)
          protocolRacer.isMaster = true;

        protocolRacer.level = character.level();
        protocolRacer.oid = racer.oid;
        protocolRacer.uid = character.uid();
        protocolRacer.name = character.name();
        protocolRacer.isReady = racer.state == tracker::RaceTracker::Racer::State::Waiting;
        protocolRacer.isHidden = false;
        protocolRacer.isNPC = false;

        protocolRacer.avatar = protocol::Avatar{};

        protocol::BuildProtocolCharacter(
          protocolRacer.avatar->character, character);

        // Build the character equipment.
        protocol::BuildProtocolItems(
          protocolRacer.avatar->equipment,
          *_serverInstance.GetDataDirector().GetItemCache().Get(
            character.characterEquipment()));

        // Build the mount equipment.
        protocol::BuildProtocolItems(
          protocolRacer.avatar->equipment,
          *_serverInstance.GetDataDirector().GetItemCache().Get(
            character.mountEquipment()));

        const auto mountRecord = GetServerInstance().GetDataDirector().GetHorseCache().Get(
          character.mountUid());
        mountRecord->Immutable(
          [&protocolRacer](const data::Horse& mount)
          {
            protocol::BuildProtocolHorse(protocolRacer.avatar->mount, mount);
          });
      });

    if (characterUid == clientContext.characterUid)
    {
      joiningRacer = protocolRacer;
    }
  }

  _commandServer.QueueCommand<decltype(response)>(
    clientId,
    [response]()
    {
      return response;
    });

  protocol::AcCmdCREnterRoomNotify notify{
    .racer = joiningRacer,
    .averageTimeRecord = clientContext.characterUid};

  for (const ClientId& roomClientId : roomInstance.clients)
  {
    _commandServer.QueueCommand<decltype(notify)>(
      roomClientId,
      [notify]()
      {
        return notify;
      });
  }

  roomInstance.clients.insert(clientId);
}

void RaceDirector::HandleChangeRoomOptions(
  ClientId clientId,
  const protocol::AcCmdCRChangeRoomOptions& command)
{
  // todo: validate command fields
  const auto& clientContext = _clients[clientId];
  auto& room = _serverInstance.GetRoomSystem().GetRoom(
    clientContext.roomUid);
  auto& roomInstance = _roomInstances[clientContext.roomUid];

  const std::bitset<6> options(
    static_cast<uint16_t>(command.optionsBitfield));

  if (options.test(0))
    room.name = command.name;
  if (options.test(1))
    room.playerCount = command.playerCount;
  if (options.test(2))
    room.password = command.password;
  if (options.test(3))
    room.gameMode = command.gameMode;
  if (options.test(4))
    room.mapBlockId = command.mapBlockId;
  if (options.test(5))
    room.unk3 = command.npcRace;

  protocol::AcCmdCRChangeRoomOptionsNotify notify{
    .optionsBitfield = command.optionsBitfield,
    .name = command.name,
    .playerCount = command.playerCount,
    .password = command.password,
    .gameMode = command.gameMode,
    .mapBlockId = command.mapBlockId,
    .npcRace = command.npcRace};

  for (const auto roomClientId : roomInstance.clients)
  {
    _commandServer.QueueCommand<decltype(notify)>(
      roomClientId,
      [notify]()
      {
        return notify;
      });
  }
}

void RaceDirector::HandleChangeTeam(
  ClientId clientId,
  const protocol::AcCmdCRChangeTeam& command)
{
  const auto& clientContext = _clients[clientId];
  auto& roomInstance = _roomInstances[clientContext.roomUid];

  auto& racer = roomInstance.tracker.GetRacer(
    clientContext.characterUid);

  // todo: team balancing

  switch (command.teamColor)
  {
    case protocol::TeamColor::Red:
      racer.team = tracker::RaceTracker::Racer::Team::Red;
      break;
    case protocol::TeamColor::Blue:
      racer.team = tracker::RaceTracker::Racer::Team::Blue;
      break;
    default: {}
  }

  protocol::AcCmdCRChangeTeamOK response{
    .characterOid = command.characterOid,
    .teamColor = command.teamColor};

  protocol::AcCmdCRChangeTeamNotify notify{
    .characterOid = command.characterOid,
    .teamColor = command.teamColor};

  _commandServer.QueueCommand<decltype(response)>(
    clientId,
    [response]()
    {
      return response;
    });

  // Notify all other clients in the room
  for (const ClientId& roomClientId : roomInstance.clients)
  {
    if (roomClientId == clientId)
      continue;

    _commandServer.QueueCommand<decltype(notify)>(
      roomClientId,
      [notify]()
      {
        return notify;
      });
  }
}

void RaceDirector::HandleLeaveRoom(ClientId clientId)
{
  protocol::AcCmdCRLeaveRoomOK response{};

  auto& clientContext = _clients[clientId];
  if (clientContext.roomUid == 0)
    return;

  auto& roomInstance = _roomInstances[clientContext.roomUid];

  _serverInstance.GetDataDirector().GetCharacter(clientContext.characterUid).Immutable(
    [roomUid = clientContext.roomUid](
      const data::Character& character)
    {
      spdlog::info("Character '{}' has left the room {}", character.name(), roomUid);
    });

  roomInstance.tracker.RemoveRacer(
    clientContext.characterUid);
  roomInstance.clients.erase(clientId);

  // Check if the leaving player was the leader
  const bool wasLeader = roomInstance.masterUid == clientContext.characterUid;

  {
    // Notify other clients in the room about the character leaving.
    protocol::AcCmdCRLeaveRoomNotify notify{
      .characterId = clientContext.characterUid,
      .unk0 = 1};

    for (const ClientId& roomClientId : roomInstance.clients)
    {
      if (roomClientId == clientId)
        continue;

      _commandServer.QueueCommand<decltype(notify)>(
        roomClientId,
        [notify]()
        {
          return notify;
        });
    }
  }

  if (not roomInstance.tracker.GetRacers().empty())
  {
    if (wasLeader)
    {
      // Find the next leader.
      // todo: assign mastership to the best player

      roomInstance.masterUid = roomInstance.tracker.GetRacers().begin()->first;

      spdlog::info("Character {} became the master of room {} after the previous master left",
        roomInstance.masterUid,
        clientContext.roomUid);

      {
        // Notify other clients in the room about the new master.
        protocol::AcCmdCRChangeMasterNotify notify{
          .masterUid = roomInstance.masterUid};

        for (const ClientId& roomClientId : roomInstance.clients)
        {
          _commandServer.QueueCommand<decltype(notify)>(
            roomClientId,
            [notify]()
            {
              return notify;
            });
        }
      }
    }
  }
  else
  {
    _serverInstance.GetRoomSystem().DeleteRoom(
      clientContext.roomUid);
    _roomInstances.erase(clientContext.roomUid);
  }

  clientContext.roomUid = data::InvalidUid;

  _commandServer.QueueCommand<decltype(response)>(
    clientId,
    [response]()
    {
      return response;
    });
}

void RaceDirector::HandleReadyRace(
  ClientId clientId,
  const protocol::AcCmdCRReadyRace& command)
{
  auto& clientContext = _clients[clientId];

  auto& roomInstance = _roomInstances[clientContext.roomUid];

  auto& racer = roomInstance.tracker.GetRacer(
    clientContext.characterUid);

  // Toggle the ready state.
  if (racer.state == tracker::RaceTracker::Racer::State::NotReady)
    racer.state = tracker::RaceTracker::Racer::State::Waiting;
  else if (racer.state == tracker::RaceTracker::Racer::State::Waiting)
    racer.state = tracker::RaceTracker::Racer::State::NotReady;

  protocol::AcCmdCRReadyRaceNotify response{
    .characterUid = clientContext.characterUid,
    .isReady = racer.state == tracker::RaceTracker::Racer::State::Waiting};

  for (const ClientId& roomClientId : roomInstance.clients)
  {
    _commandServer.QueueCommand<decltype(response)>(
      roomClientId,
      [response]()
      {
        return response;
      });
  }
}

void RaceDirector::HandleStartRace(
  ClientId clientId,
  const protocol::AcCmdCRStartRace& command)
{
  const auto& clientContext = _clients[clientId];
  auto& roomInstance = _roomInstances[clientContext.roomUid];

  if (clientContext.characterUid != roomInstance.masterUid)
    throw std::runtime_error("Client tried to start the race even though they're not the master");

  const auto& room = _serverInstance.GetRoomSystem().GetRoom(
    clientContext.roomUid);

  constexpr uint32_t AllMapsCourseId = 10000;
  constexpr uint32_t NewMapsCourseId = 10001;
  constexpr uint32_t HotMapsCourseId = 10002;

  if (room.mapBlockId == AllMapsCourseId
    || room.mapBlockId == NewMapsCourseId
    || room.mapBlockId == HotMapsCourseId)
  {
    // TODO: Select a random mapBlockId from a predefined list
    // For now its a map that at least loads in
    roomInstance.mapBlockId = 1;
  }
  else
  {
    roomInstance.mapBlockId = room.mapBlockId;
  }

  const protocol::AcCmdRCRoomCountdown roomCountdown{
    .countdown = 3000,
    .mapBlockId = roomInstance.mapBlockId};

  // Broadcast room countdown.
  for (const ClientId& roomClientId : roomInstance.clients)
  {
    _commandServer.QueueCommand<decltype(roomCountdown)>(
      roomClientId,
      [roomCountdown]()
      {
        return roomCountdown;
      });
  }

  roomInstance.stage = RoomInstance::Stage::Loading;
  roomInstance.stageTimeoutTimePoint = std::chrono::steady_clock::now() + std::chrono::seconds(30);

  // Queue race start after room countdown.
  _scheduler.Queue(
    [this, clientId]()
    {
      const auto& clientContext = _clients[clientId];

      const auto& room = _serverInstance.GetRoomSystem().GetRoom(
        clientContext.roomUid);
      auto& roomInstance = _roomInstances[clientContext.roomUid];

      protocol::AcCmdCRStartRaceNotify notify{
        .gameMode = room.gameMode,
        .teamMode = room.teamMode,
        .p2pRelayAddress = asio::ip::address_v4::loopback().to_uint(),
        .p2pRelayPort = static_cast<uint16_t>(10500)};

      notify.mapBlockId = roomInstance.mapBlockId;
      notify.missionId = room.missionId;

      // Build the racers.
      for (const auto& [characterUid, racer] : roomInstance.tracker.GetRacers())
      {
        std::string characterName;
        GetServerInstance().GetDataDirector().GetCharacter(characterUid).Immutable(
          [&characterName](
            const data::Character& character)
          {
            characterName = character.name();
          });

        auto& protocolRacer = notify.racers.emplace_back(
          protocol::AcCmdCRStartRaceNotify::Player{
            .oid = racer.oid,
            .name = characterName,
            .p2dId = racer.oid,});

        switch (racer.team)
        {
          case tracker::RaceTracker::Racer::Team::Solo:
            protocolRacer.teamColor = protocol::TeamColor::Solo;
            break;
          case tracker::RaceTracker::Racer::Team::Red:
            protocolRacer.teamColor = protocol::TeamColor::Red;
            break;
          case tracker::RaceTracker::Racer::Team::Blue:
            protocolRacer.teamColor = protocol::TeamColor::Blue;
            break;
        }
      }

      // Reset jump combo/star point (boost)
      for (auto& racer : roomInstance.tracker.GetRacers() | std::views::values)
      {
        racer.jumpComboValue = 0;
        racer.starPointValue = 0;
      }

      // todo: start loading timeout timer
      // Send to all clients in the room.
      for (const ClientId& roomClientId : roomInstance.clients)
      {
        const auto& roomClientContext = _clients[roomClientId];

        auto& racer = roomInstance.tracker.GetRacer(
          roomClientContext.characterUid);
        racer.state = tracker::RaceTracker::Racer::State::Loading;

        notify.hostOid = racer.oid;

        _commandServer.QueueCommand<decltype(notify)>(
          roomClientId,
          [notify]()
          {
            return notify;
          });
      }
    },
    Scheduler::Clock::now() + std::chrono::milliseconds(roomCountdown.countdown));
}

void RaceDirector::HandleRaceTimer(
  ClientId clientId,
  const protocol::AcCmdUserRaceTimer& command)
{
  protocol::AcCmdUserRaceTimerOK response{
    .clientRaceClock = command.clientClock,
    .serverRaceClock = TimePointToRaceTimePoint(
      std::chrono::steady_clock::now()),};

  _commandServer.QueueCommand<decltype(response)>(
    clientId,
    [response]()
    {
      return response;
    });
}

void RaceDirector::HandleLoadingComplete(
  ClientId clientId,
  const protocol::AcCmdCRLoadingComplete& command)
{
  auto& clientContext = _clients[clientId];
  auto& roomInstance = _roomInstances[clientContext.roomUid];

  auto& racer = roomInstance.tracker.GetRacer(
    clientContext.characterUid);

  racer.state = tracker::RaceTracker::Racer::State::Racing;

  // Notify all clients in the room that this player's loading is complete
  for (const ClientId& roomClientId : roomInstance.clients)
  {
    _commandServer.QueueCommand<protocol::AcCmdCRLoadingCompleteNotify>(
      roomClientId,
      [oid = racer.oid]()
      {
        return protocol::AcCmdCRLoadingCompleteNotify{
          .oid = oid};
      });
  }
}

void RaceDirector::HandleUserRaceFinal(
  ClientId clientId,
  const protocol::AcCmdUserRaceFinal& command)
{
  auto& clientContext = _clients[clientId];
  auto& roomInstance = _roomInstances[clientContext.roomUid];

  // todo: sanity check for course time
  // todo: address npc racers and update their states
  auto& racer = roomInstance.tracker.GetRacer(
    clientContext.characterUid);

  racer.state = tracker::RaceTracker::Racer::State::Finishing;
  racer.courseTime = command.courseTime;

  protocol::AcCmdUserRaceFinalNotify notify{
    .oid = racer.oid,
    .courseTime = command.courseTime};

  // todo: start finish timeout timer

  for (const ClientId& roomClientId : roomInstance.clients)
  {
    _commandServer.QueueCommand<decltype(notify)>(
      roomClientId,
      [notify]()
      {
        return notify;
      });
  }
}

void RaceDirector::HandleRaceResult(
  ClientId clientId,
  const protocol::AcCmdCRRaceResult& command)
{
  // todo: only requested by the room master

  auto& clientContext = _clients[clientId];
  auto& roomInstance = _roomInstances[clientContext.roomUid];

  protocol::AcCmdCRRaceResultOK response{
    .member1 = 1,
    .member2 = 1,
    .member3 = 1,
    .member4 = 1,
    .member5 = 1,
    .member6 = 1};

  _commandServer.QueueCommand<decltype(response)>(
    clientId,
    [response]()
    {
      return response;
    });
}

void RaceDirector::HandleP2PRaceResult(
  ClientId clientId,
  const protocol::AcCmdCRP2PResult& command)
{
  const auto& clientContext = _clients[clientId];
  auto& roomInstance = _roomInstances[clientContext.roomUid];

  protocol::AcCmdGameRaceP2PResult result{};
  for (const auto & [uid, racer] : roomInstance.tracker.GetRacers())
  {
    auto& protocolRacer = result.member1.emplace_back();
    protocolRacer.oid = racer.oid;
  }

  _commandServer.QueueCommand<decltype(result)>(clientId, [result](){return result;});
}

void RaceDirector::HandleP2PUserRaceResult(
  ClientId clientId,
  const protocol::AcCmdUserRaceP2PResult& command)
{
}

void RaceDirector::HandleAwardStart(
  ClientId clientId,
  const protocol::AcCmdCRAwardStart& command)
{
  const auto& clientContext = _clients[clientId];
  const auto& roomInstance = _roomInstances[clientContext.roomUid];

  protocol::AcCmdRCAwardNotify notify{
    .member1 = command.member1};

  for (const auto roomClientId : roomInstance.clients)
  {
    _commandServer.QueueCommand<decltype(notify)>(
      roomClientId,
      [notify]()
      {
        return notify;
      });
  }
}

void RaceDirector::HandleAwardEnd(
  ClientId clientId,
  const protocol::AcCmdCRAwardEnd& command)
{
  const auto& clientContext = _clients[clientId];
  const auto& roomInstance = _roomInstances[clientContext.roomUid];

  protocol::AcCmdCRAwardEndNotify notify{};

  for (const auto roomClientId : roomInstance.clients)
  {
    if (roomClientId == clientId)
      continue;

    _commandServer.QueueCommand<decltype(notify)>(
      roomClientId,
      [notify]()
      {
        return notify;
      });
  }
}

void RaceDirector::HandleStarPointGet(
  ClientId clientId,
  const protocol::AcCmdCRStarPointGet& command)
{
  const auto& clientContext = _clients[clientId];
  auto& roomInstance = _roomInstances[clientContext.roomUid];

  auto& racer = roomInstance.tracker.GetRacer(
    clientContext.characterUid);
  if (command.characterOid != racer.oid)
  {
    throw std::runtime_error(
      "Client tried to perform action on behalf of different racer");
  }
  
  // Get pointer and if inserted with characterOid as key
  const auto& room = _serverInstance.GetRoomSystem().GetRoom(
    clientContext.roomUid);
  const auto& gameModeTemplate = GetServerInstance().GetCourseRegistry().GetCourseGameModeInfo(
    room.gameMode);

  racer.starPointValue = std::min(
    racer.starPointValue + command.gainedStarPoints,
    gameModeTemplate.starPointsMax);

  // Star point get (boost get) is only called in speed, should never give magic item
  protocol::AcCmdCRStarPointGetOK response{
    .characterOid = command.characterOid,
    .starPointValue = racer.starPointValue,
    .giveMagicItem = false
  };

  _commandServer.QueueCommand<decltype(response)>(
    clientId,
    [clientId, response]()
    {
      return response;
    });
}

void RaceDirector::HandleRequestSpur(
  ClientId clientId,
  const protocol::AcCmdCRRequestSpur& command)
{
  const auto& clientContext = _clients[clientId];
  auto& roomInstance = _roomInstances[clientContext.roomUid];

  auto& racer = roomInstance.tracker.GetRacer(
    clientContext.characterUid);
  if (command.characterOid != racer.oid)
  {
    throw std::runtime_error(
      "Client tried to perform action on behalf of different racer");
  }

  const auto& room = _serverInstance.GetRoomSystem().GetRoom(
    clientContext.roomUid);
  const auto& gameModeTemplate = GetServerInstance().GetCourseRegistry().GetCourseGameModeInfo(
    room.gameMode);

  if (racer.starPointValue < gameModeTemplate.spurConsumeStarPoints)
    throw std::runtime_error("Client is dead ass cheating (or is really desynced)");

  racer.starPointValue -= gameModeTemplate.spurConsumeStarPoints;

  protocol::AcCmdCRRequestSpurOK response{
    .characterOid = command.characterOid,
    .activeBoosters = command.activeBoosters,
    .startPointValue = racer.starPointValue,
    .comboBreak = command.comboBreak};

  _commandServer.QueueCommand<decltype(response)>(
    clientId,
    [clientId, response]()
    {
      return response;
    });
}

void RaceDirector::HandleHurdleClearResult(
  ClientId clientId,
  const protocol::AcCmdCRHurdleClearResult& command)
{
  const auto& clientContext = _clients[clientId];
  auto& roomInstance = _roomInstances[clientContext.roomUid];

  auto& racer = roomInstance.tracker.GetRacer(
    clientContext.characterUid);
  if (command.characterOid != racer.oid)
  {
    throw std::runtime_error(
      "Client tried to perform action on behalf of different racer");
  }

  protocol::AcCmdCRHurdleClearResultOK response{
    .characterOid = command.characterOid,
    .hurdleClearType = command.hurdleClearType,
    .jumpCombo = 0,
    .unk3 = 0
  };
  
  // Give magic item is calculated later
  protocol::AcCmdCRStarPointGetOK starPointResponse{
    .characterOid = command.characterOid,
    .starPointValue = racer.starPointValue,
    .giveMagicItem = false
  };

  const auto& room = _serverInstance.GetRoomSystem().GetRoom(
    clientContext.roomUid);
  const auto& gameModeTemplate = GetServerInstance().GetCourseRegistry().GetCourseGameModeInfo(
    room.gameMode);

  switch (command.hurdleClearType)
  {
    case protocol::AcCmdCRHurdleClearResult::HurdleClearType::Perfect:
    {
      // Perfect jump over the hurdle.
      racer.jumpComboValue = std::min(
        static_cast<uint32_t>(99),
        racer.jumpComboValue + 1);

      if (room.gameMode == 1)
      {
        // Only send jump combo if it is a speed race
        response.jumpCombo = racer.jumpComboValue;
      }

      // Calculate max applicable combo
      const auto& applicableComboCount = std::min(
        gameModeTemplate.perfectJumpMaxBonusCombo,
        racer.jumpComboValue);
      // Calculate max combo count * perfect jump boost unit points
      const auto& gainedStarPointsFromCombo = applicableComboCount * gameModeTemplate.perfectJumpUnitStarPoints;
      // Add boost points to character boost tracker
      racer.starPointValue = std::min(
        racer.starPointValue + gameModeTemplate.perfectJumpStarPoints + gainedStarPointsFromCombo,
        gameModeTemplate.starPointsMax);

      // Update boost gauge
      starPointResponse.starPointValue = racer.starPointValue;
      break;
    }
    case protocol::AcCmdCRHurdleClearResult::HurdleClearType::Good:
    case protocol::AcCmdCRHurdleClearResult::HurdleClearType::DoubleJumpOrGlide:
    {
      // Not a perfect jump over the hurdle, reset the jump combo.
      racer.jumpComboValue = 0;
      response.jumpCombo = racer.jumpComboValue;

      // Increment boost gauge by a good jump
      racer.starPointValue = std::min(
        racer.starPointValue + gameModeTemplate.goodJumpStarPoints,
        gameModeTemplate.starPointsMax);

      // Update boost gauge
      starPointResponse.starPointValue = racer.starPointValue;
      break;
    }
    case protocol::AcCmdCRHurdleClearResult::HurdleClearType::Collision:
    {
      // A collision with hurdle, reset the jump combo.
      racer.jumpComboValue = 0;
      response.jumpCombo = racer.jumpComboValue;
      break;
    }
    default:
    {
      spdlog::warn("Unhandled hurdle clear type {}",
        static_cast<uint8_t>(command.hurdleClearType));
      return;
    }
  }
  
  // Needs to be assigned after hurdle clear result calculations
  // Triggers magic item request when set to true (if gamemode is magic and magic gauge is max)
  // TODO: is there only perfect clears in magic race?
  starPointResponse.giveMagicItem = 
    room.gameMode == 2 &&
    racer.starPointValue >= gameModeTemplate.starPointsMax &&
    command.hurdleClearType == protocol::AcCmdCRHurdleClearResult::HurdleClearType::Perfect;

  // Update the star point value if the jump was not a collision.
  if (command.hurdleClearType != protocol::AcCmdCRHurdleClearResult::HurdleClearType::Collision)
  {
    _commandServer.QueueCommand<decltype(starPointResponse)>(
      clientId,
      [clientId, starPointResponse]()
      {
        return starPointResponse;
      });
  }

  _commandServer.QueueCommand<decltype(response)>(
    clientId,
    [clientId, response]()
    {
      return response;
    });
}

void RaceDirector::HandleStartingRate(
  ClientId clientId,
  const protocol::AcCmdCRStartingRate& command)
{
  // TODO: check for sensible values
  if (command.unk1 < 1 && command.boostGained < 1)
  {
    // Velocity and boost gained is not valid
    // TODO: throw?
    return;
  }

  const auto& clientContext = _clients[clientId];
  auto& roomInstance = _roomInstances[clientContext.roomUid];

  auto& racer = roomInstance.tracker.GetRacer(
    clientContext.characterUid);
  if (command.characterOid != racer.oid)
  {
    throw std::runtime_error(
      "Client tried to perform action on behalf of different racer");
  }

  const auto& room = _serverInstance.GetRoomSystem().GetRoom(
    clientContext.roomUid);
  const auto& gameModeTemplate = GetServerInstance().GetCourseRegistry().GetCourseGameModeInfo(
    room.gameMode);

  // TODO: validate boost gained against a table and determine good/perfect start
  racer.starPointValue = std::min(
    racer.starPointValue + command.boostGained,
    gameModeTemplate.starPointsMax);

  // Only send this on good/perfect starts
  protocol::AcCmdCRStarPointGetOK response{
    .characterOid = command.characterOid,
    .starPointValue = racer.starPointValue,
    .giveMagicItem = false // TODO: this would never give a magic item on race start, right?
  };

  _commandServer.QueueCommand<decltype(response)>(
    clientId,
    [clientId, response]()
    {
      return response;
    });
}

void RaceDirector::HandleRaceUserPos(
  ClientId clientId,
  const protocol::AcCmdUserRaceUpdatePos& command)
{
  const auto& clientContext = _clients[clientId];
  auto& roomInstance = _roomInstances[clientContext.roomUid];
  auto& racer = roomInstance.tracker.GetRacer(clientContext.characterUid);

  if (command.oid != racer.oid)
  {
    throw std::runtime_error(
      "Client tried to perform action on behalf of different racer");
  }

  const auto& room = _serverInstance.GetRoomSystem().GetRoom(
    clientContext.roomUid);
  const auto& gameModeTemplate = GetServerInstance().GetCourseRegistry().GetCourseGameModeInfo(
    room.gameMode);

  // Only regenerate magic during active race (after countdown finishes)
  // Check if game mode is magic, race is active, countdown finished, and not holding an item
  const bool raceActuallyStarted = std::chrono::steady_clock::now() >= roomInstance.raceStartTimePoint;
  
  if (room.gameMode == 2
    && racer.state == tracker::RaceTracker::Racer::State::Racing
    && raceActuallyStarted
    && not racer.magicItem.has_value())
  {
    if (racer.starPointValue < gameModeTemplate.starPointsMax)
    {
      // TODO: add these to configuration somewhere
      // Eyeballed these values from watching videos
      constexpr uint32_t NoItemHeldBoostAmount = 2000;
      // TODO: does holding an item and with certain equipment give you magic? At a reduced rate?
      constexpr uint32_t ItemHeldWithEquipmentBoostAmount = 1000;
      racer.starPointValue = std::min(gameModeTemplate.starPointsMax, racer.starPointValue + NoItemHeldBoostAmount);
    }

    // Conditional already checks if there is no magic item and gamemode is magic,
    // only check if racer has max magic gauge to give magic item
    protocol::AcCmdCRStarPointGetOK starPointResponse{
      .characterOid = command.oid,
      .starPointValue = racer.starPointValue,
      .giveMagicItem = racer.starPointValue >= gameModeTemplate.starPointsMax
    };

    _commandServer.QueueCommand<decltype(starPointResponse)>(
      clientId,
      [starPointResponse]
      {
        return starPointResponse;
      });
  }

  for (const auto& roomClientId : roomInstance.clients)
  {
    // Prevent broadcast to self.
    if (clientId == roomClientId)
      continue;
  }
}

void RaceDirector::HandleChat(ClientId clientId, const protocol::AcCmdCRChat& command)
{
  const auto& clientContext = _clients[clientId];

  const auto messageVerdict = _serverInstance.GetChatSystem().ProcessChatMessage(
    clientContext.characterUid, command.message);

  const auto characterRecord = _serverInstance.GetDataDirector().GetCharacter(
    clientContext.characterUid);

  protocol::AcCmdCRChatNotify notify{
    .message = messageVerdict.message,
    .isSystem = false};

  characterRecord.Immutable([&notify](const data::Character& character)
  {
    notify.author = character.name();
  });

  spdlog::info("[Room {}] {}: {}", clientContext.roomUid, notify.author, notify.message);

  const auto& roomInstance = _roomInstances[clientContext.roomUid];
  for (const ClientId roomClientId : roomInstance.clients)
  {
    _commandServer.QueueCommand<decltype(notify)>(
      roomClientId,
      [notify]{return notify;});
  }
}

void RaceDirector::HandleRelayCommand(
  ClientId clientId,
  const protocol::AcCmdCRRelayCommand& command)
{
  const auto& clientContext = _clients[clientId];
  
  // Create relay notify message
  protocol::AcCmdCRRelayCommandNotify notify{
    .member1 = command.member1,
    .member2 = command.member2};

  // Get the room instance for this client
  const auto& roomInstance = _roomInstances[clientContext.roomUid];
  
  // Relay the command to all other clients in the room
  for (const ClientId roomClientId : roomInstance.clients)
  {
    if (roomClientId != clientId) // Don't send back to sender
    {
      _commandServer.QueueCommand<decltype(notify)>(
        roomClientId,
        [notify]{return notify;});
    }
  }
}

void RaceDirector::HandleRelay(
  ClientId clientId,
  const protocol::AcCmdCRRelay& command)
{
  const auto& clientContext = _clients[clientId];
  
  // Create relay notify message
  protocol::AcCmdCRRelayNotify notify{
    .oid = command.oid,
    .member2 = command.member2,
    .member3 = command.member3,
    .data = std::move(command.data),};

  // Get the room instance for this client
  const auto& roomInstance = _roomInstances[clientContext.roomUid];
  
  // Relay the command to all other clients in the room
  for (const ClientId roomClientId : roomInstance.clients)
  {
    if (roomClientId != clientId) // Don't send back to sender
    {
      _commandServer.QueueCommand<decltype(notify)>(
        roomClientId,
        [notify]{return notify;});
    }
  }
}

void RaceDirector::HandleUserRaceActivateInteractiveEvent
(
  ClientId clientId,
  const protocol::AcCmdUserRaceActivateInteractiveEvent& command)
{
  const auto& clientContext = _clients[clientId];
  auto& roomInstance = _roomInstances[clientContext.roomUid];

  // Get the sender's OID from the room tracker
  auto& racer = roomInstance.tracker.GetRacer(clientContext.characterUid);

  protocol::AcCmdUserRaceActivateInteractiveEvent notify{
    .member1 = command.member1,
    .characterOid = racer.oid, // sender oid
    .member3 = command.member3
  };

  // Broadcast to all clients in the room
  for (const ClientId roomClientId : roomInstance.clients)
  {
    _commandServer.QueueCommand<decltype(notify)>(
      roomClientId,
      [notify]{return notify;});
  }
}

void RaceDirector::HandleUserRaceActivateEvent
(
  ClientId clientId,
  const protocol::AcCmdUserRaceActivateEvent& command)
{
  const auto& clientContext = _clients[clientId];
  auto& roomInstance = _roomInstances[clientContext.roomUid];

  // Get the sender's OID from the room tracker
  auto& racer = roomInstance.tracker.GetRacer(clientContext.characterUid);

  spdlog::info("HandleUserRaceActivateEvent: clientId={}, eventId={}, characterOid={}", 
    clientId, command.eventId, racer.oid);

  protocol::AcCmdUserRaceActivateEvent notify{
    .eventId = command.eventId,
    .characterOid = racer.oid, // sender oid
  };

  // Broadcast to all clients in the room
  for (const ClientId roomClientId : roomInstance.clients)
  {
    _commandServer.QueueCommand<decltype(notify)>(
      roomClientId,
      [notify]{return notify;});
  }
}

void RaceDirector::HandleRequestMagicItem(
  ClientId clientId,
  const protocol::AcCmdCRRequestMagicItem& command)
{
  spdlog::info("Player {} requested magic item (OID: {}, type: {})", 
    clientId, command.member1, command.member2);

  const auto& clientContext = _clients[clientId];
  auto& roomInstance = _roomInstances[clientContext.roomUid];
  auto& racer = roomInstance.tracker.GetRacer(clientContext.characterUid);

  // TODO: command.member1 is character oid?
  if (command.member1 != racer.oid)
  {
    // TODO: throw? return?
    return;
  }

  // Check if racer is already holding a magic item
  if (racer.magicItem.has_value())
  {
    // Has no corresponding cancel, log and return
    spdlog::warn("Character {} tried to request a magic item in race {} but they already have one, skipping...",
      clientContext.characterUid,
      clientContext.roomUid);
    return;
  }

  // TODO: reset magic gauge to 0?
  protocol::AcCmdCRStarPointGetOK starPointResponse{
    .characterOid = command.member1,
    .starPointValue = racer.starPointValue = 0,
    .giveMagicItem = false
  };

  _commandServer.QueueCommand<decltype(starPointResponse)>(
    clientId,
    [starPointResponse]
    {
      return starPointResponse;
    });

  // 2 - Bolt
  // 4 - Shield
  // 10 - Ice wall
  protocol::AcCmdCRRequestMagicItemOK response{
    .member1 = command.member1,
    .member2 = racer.magicItem.emplace(2),
    .member3 = 0
  };

  _commandServer.QueueCommand<decltype(response)>(
    clientId,
    [response]
    {
      return response;
    });

  protocol::AcCmdCRRequestMagicItemNotify notify{
    .member1 = response.member2,
    .member2 = response.member1
  };

  for (const auto& roomClientId : roomInstance.clients)
  {
    // Prevent broadcast to self
    if (roomClientId == clientId)
      continue;
    
    _commandServer.QueueCommand<decltype(notify)>(
      roomClientId,
      [notify]
      {
        return notify;
      });
  }
}

void RaceDirector::HandleUseMagicItem(
  ClientId clientId,
  const protocol::AcCmdCRUseMagicItem& command)
{
  spdlog::info("Player {} used magic item {} (OID: {})", clientId, command.mode, command.type_copy);
  const auto& clientContext = _clients[clientId];
  auto& roomInstance = _roomInstances[clientContext.roomUid];
  auto& racer = roomInstance.tracker.GetRacer(clientContext.characterUid);

  if (command.type_copy != racer.oid)
  {
    spdlog::warn("Client {} tried to use magic on behalf of different racer", clientId);
    return;
  }

  // Validate base magic type (convert crit types to base types for validation)
  uint32_t baseMagicType = command.mode;
  if (baseMagicType % 2 != 0)
  {
    // Client sent odd number (crit variant) - convert to base type for validation
    baseMagicType -= 1;  // 3→2, 5→4, 7→6, etc.
  }

  // Validate player actually has this magic item
  if (!racer.magicItem.has_value())
  {
    spdlog::warn("Client {} tried to use magic but doesn't have any magic item", clientId);
    return;
  }

  if (racer.magicItem.value() != baseMagicType)
  {
    spdlog::warn("Client {} tried to use magic type {} but has magic type {}", 
      clientId, baseMagicType, racer.magicItem.value());
    return;
  }

  // Calculate critical hit based on horse ambition stat
  uint32_t finalMagicType = baseMagicType;
  
  const auto characterRecord = GetServerInstance().GetDataDirector().GetCharacter(
    clientContext.characterUid);
  
  characterRecord.Immutable([this, &finalMagicType, baseMagicType](const data::Character& character)
  {
    const auto horseRecord = GetServerInstance().GetDataDirector().GetHorseCache().Get(
      character.mountUid());
    
    horseRecord->Immutable([&finalMagicType, baseMagicType](const data::Horse& horse)
    {
      // Flat 15% critical hit chance for all magic
      // TODO: Find out the crit hit chance used in OG Alicia and change the numbers
      const float critChance = 0.15f;
      
      // Roll for critical
      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_real_distribution<float> dist(0.0f, 1.0f);
      
      float roll = dist(gen);
      if (roll < critChance)
      {
        finalMagicType = baseMagicType + 1;  // Convert to crit variant
        spdlog::info("CRITICAL MAGIC! Roll: {:.3f} < {:.1f}% chance (type {} → {})", 
          roll, critChance * 100.0f, baseMagicType, finalMagicType);
      }
      else
      {
        spdlog::info("Normal magic. Roll: {:.3f} >= {:.1f}% chance (type {})", 
          roll, critChance * 100.0f, baseMagicType);
      }
    });
  });

  // For bolt magic, send targeting notify FIRST to prime the victim
  if (baseMagicType == 2 && command.idsBlock.has_value() && command.idsBlock->ids_count > 0)
  {
    int16_t targetOid = command.idsBlock->ids[0];
    
    protocol::AcCmdCRChangeMagicTargetNotify preTargetNotify{
      .v0 = static_cast<uint16_t>(command.type_copy),  // Attacker OID
      .v1 = 0,                                         // Old target (0 = none)
      .v2 = static_cast<uint16_t>(targetOid),         // New target OID
      .v3 = static_cast<uint16_t>(baseMagicType)      // Magic type
    };
    
    spdlog::info("=== BOLT: Sending TargetNotify FIRST ===");
    spdlog::info("  v0 (Attacker OID): {}", preTargetNotify.v0);
    spdlog::info("  v1 (Old Target): {}", preTargetNotify.v1);
    spdlog::info("  v2 (New Target OID): {}", preTargetNotify.v2);
    spdlog::info("  v3 (Magic Type): {}", preTargetNotify.v3);
    spdlog::info("  Broadcasting to {} clients in room", roomInstance.clients.size());
    
    for (const auto& roomClientId : roomInstance.clients)
    {
      const auto& ctx = _clients[roomClientId];
      auto& r = roomInstance.tracker.GetRacer(ctx.characterUid);
      spdlog::info("    → Client {} (CharUID: {}, OID: {})", 
        roomClientId, ctx.characterUid, r.oid);
      
      _commandServer.QueueCommand<decltype(preTargetNotify)>(
        roomClientId, 
        [preTargetNotify]() { return preTargetNotify; });
    }
  }

  // Send OK response with SERVER-CALCULATED magic type
  protocol::AcCmdCRUseMagicItemOK response{};
  response.actor_oid = static_cast<uint16_t>(command.type_copy);  // Caster OID
  response.magic_type = finalMagicType;  // Use calculated type (may be crit)
  
  // Echo extra timing/sync fields from client
  response.extraA = command.extraA;
  response.extraB = command.extraB;
  response.extraF = command.extraF;

  // Set spatial data for modes 10/11 (ice wall)
  if (baseMagicType == 10)
  {
    if (command.vecA.has_value() && command.vecB.has_value())
    {
      protocol::Spatial spatial;
      spatial.vecA = {command.vecA->x, command.vecA->y, command.vecA->z};
      spatial.vecB = {command.vecB->x, command.vecB->y, command.vecB->z};
      response.spatial = spatial;
    }
    else
    {
      protocol::Spatial spatial;
      spatial.vecA = {0.0f, 0.0f, 0.0f};
      spatial.vecB = {0.0f, 0.0f, 0.0f};
      response.spatial = spatial;
    }
  }
  else
  {
    response.spatial = std::nullopt;
  }

  // Set common payload (always present)
  response.payload.ids_count = 0;
  
  if (command.idsBlock.has_value())
  {
    const auto& idsBlock = command.idsBlock.value();
    response.payload.ids_count = std::min(static_cast<uint8_t>(idsBlock.ids_count), static_cast<uint8_t>(8));
    for (uint8_t i = 0; i < response.payload.ids_count; ++i)
    {
      response.payload.ids[i] = static_cast<int16_t>(idsBlock.ids[i]);
    }
  }
  
  // Zero out unused IDs
  for (uint8_t i = response.payload.ids_count; i < 8; ++i)
  {
    response.payload.ids[i] = 0;
  }

  // Tail fields
  // Set tail_u16 to victim OID for bolt, otherwise attacker OID
  if (baseMagicType == 2 && response.payload.ids_count > 0)
  {
    response.tail_u16 = static_cast<uint16_t>(response.payload.ids[0]);  // Victim OID for bolt
  }
  else
  {
    response.tail_u16 = static_cast<uint16_t>(command.type_copy);  // Attacker OID for other magic
  }
  response.tail_u32 = 0;

  _commandServer.QueueCommand<decltype(response)>(
    clientId,
    [response]
    {
      return response;
    });

  // Notify other players AND apply effects
  protocol::AcCmdCRUseMagicItemNotify usageNotify{};
  
  usageNotify.actor_oid = static_cast<uint16_t>(command.type_copy);
  usageNotify.magic_type = finalMagicType;

  // Set spatial data for modes 10/11 (ice wall)
  if (baseMagicType == 10)
  {
    if (command.vecA.has_value() && command.vecB.has_value())
    {
      protocol::Spatial spatial;
      spatial.vecA = {command.vecA->x, command.vecA->y, command.vecA->z};
      spatial.vecB = {command.vecB->x, command.vecB->y, command.vecB->z};
      usageNotify.spatial = spatial;
    }
    else
    {
      protocol::Spatial spatial;
      spatial.vecA = {0.0f, 0.0f, 0.0f};
      spatial.vecB = {0.0f, 0.0f, 0.0f};
      usageNotify.spatial = spatial;
    }
  }
  else
  {
    usageNotify.spatial = std::nullopt;
  }

  // Set common payload (always present)
  usageNotify.payload.ids_count = 0;
  
  if (command.idsBlock.has_value())
  {
    const auto& idsBlock = command.idsBlock.value();
    usageNotify.payload.ids_count = std::min(static_cast<uint8_t>(idsBlock.ids_count), static_cast<uint8_t>(8));       
    for (uint8_t i = 0; i < usageNotify.payload.ids_count; ++i)
    {
      usageNotify.payload.ids[i] = static_cast<int16_t>(idsBlock.ids[i]);
    }

    // For bolt with targets, apply effects server-side
    if (baseMagicType == 2 && usageNotify.payload.ids_count > 0)
    {
      int16_t targetOid = usageNotify.payload.ids[0];
      spdlog::info("=== BOLT HIT PROCESSING ===");
      spdlog::info("  Target OID: {}", targetOid);
      spdlog::info("  Is Critical: {}", finalMagicType == 3);

      // Remove target's magic item if they have one
      for (auto& [targetUid, targetRacer] : roomInstance.tracker.GetRacers())
      {
        if (targetRacer.oid == targetOid && targetRacer.magicItem.has_value())
        {
          uint32_t lostItemId = targetRacer.magicItem.value();
          spdlog::info("  → Target OID {} (UID {}) lost magic item {}", 
            targetOid, targetUid, lostItemId);
          targetRacer.magicItem.reset();
          
          // Send explicit MagicExpire command to victim
          protocol::AcCmdRCMagicExpire expireNotify{};
          expireNotify.magicId = lostItemId;
          expireNotify.actorId = static_cast<uint16_t>(targetOid);
          expireNotify.magicCode = static_cast<uint16_t>(lostItemId);
          expireNotify.reasonFlag = 1;  // Hit by bolt (reason: consumed/stolen)
          
          // Find victim's client and send
          for (const auto& roomClientId : roomInstance.clients)
          {
            const auto& roomClientContext = _clients[roomClientId];
            auto& roomClientRacer = roomInstance.tracker.GetRacer(roomClientContext.characterUid);
            if (roomClientRacer.oid == targetOid)
            {
              _commandServer.QueueCommand<decltype(expireNotify)>(
                roomClientId,
                [expireNotify]() { return expireNotify; });
              spdlog::info("Sent MagicExpire to victim client {} (OID {})", roomClientId, targetOid);
              break;
            }
          }
          break;
        }
      }

      // Consume attacker's bolt
      racer.magicItem.reset();
      spdlog::info("  → Attacker OID {} consumed bolt", command.type_copy);
    }
  }
  
  // Zero out unused IDs
  for (uint8_t i = usageNotify.payload.ids_count; i < 8; ++i)
  {
    usageNotify.payload.ids[i] = 0;
  }

  // Tail fields
  // Set tail_u16 to victim OID if bolt has targets, otherwise attacker OID
  if (baseMagicType == 2 && usageNotify.payload.ids_count > 0)
  {
    usageNotify.tail_u16 = static_cast<uint16_t>(usageNotify.payload.ids[0]);  // Victim OID for bolt
  }
  else
  {
    usageNotify.tail_u16 = static_cast<uint16_t>(command.type_copy);  // Attacker OID for other magic
  }
  usageNotify.tail_u32 = 0;

  // Send usage notification to ALL players (including attacker for bolt)
  // Skip only for ice wall (has its own handling)
  if (baseMagicType != 10) 
  {
    spdlog::info("=== Broadcasting UseMagicItemNotify ===");
    spdlog::info("  Actor OID: {}, Magic Type: {}, Target Count: {}", 
      usageNotify.actor_oid, usageNotify.magic_type, usageNotify.payload.ids_count);
    if (usageNotify.payload.ids_count > 0)
    {
      spdlog::info("  Target OID: {}", usageNotify.payload.ids[0]);
    }
    spdlog::info("  Broadcasting to {} clients", roomInstance.clients.size());
    
    for (const auto& roomClientId : roomInstance.clients)
    {
      const auto& ctx = _clients[roomClientId];
      auto& r = roomInstance.tracker.GetRacer(ctx.characterUid);
      spdlog::info("    → Client {} (CharUID: {}, OID: {})", 
        roomClientId, ctx.characterUid, r.oid);
      
      _commandServer.QueueCommand<decltype(usageNotify)>(
        roomClientId,
        [usageNotify]() { return usageNotify; });
    }
  }

  // Special handling for ice wall
  if (baseMagicType == 10)
  {
    spdlog::info("Ice wall used! Spawning ice wall at player {} location", clientId);     

    protocol::AcCmdCRUseMagicItemNotify notify{};
    notify.actor_oid = static_cast<uint16_t>(command.type_copy);  // Caster OID
    notify.magic_type = finalMagicType;  // Use calculated type (may be crit)
    
    // NOTE: Notify packet does NOT have extraA/extraB/extraF fields (unlike UseMagicItemOK)

    // Set spatial data for ice wall (mode 10)
    if (command.vecA.has_value() && command.vecB.has_value())
    {
      protocol::Spatial spatial;
      spatial.vecA = {command.vecA->x, command.vecA->y, command.vecA->z};
      spatial.vecB = {command.vecB->x, command.vecB->y, command.vecB->z};
      notify.spatial = spatial;
    }
    else
    {
      protocol::Spatial spatial;
      spatial.vecA = {0.0f, 0.0f, 0.0f};
      spatial.vecB = {0.0f, 0.0f, 0.0f};
      notify.spatial = spatial;
    }

    // Set common payload (always present, but empty for ice wall)
    notify.payload.ids_count = 0;
    for (uint8_t i = 0; i < 8; ++i)
    {
      notify.payload.ids[i] = 0;
    }

    // Tail fields
    notify.tail_u16 = static_cast<uint16_t>(command.type_copy);
    notify.tail_u32 = 0;
    // Spawn ice wall at a reasonable position (near start line like other items)
    auto& iceWall = roomInstance.tracker.AddItem();
    iceWall.itemType = 102;  // Use same type as working items (temporarily)
    iceWall.position = {25.0f, -25.0f, -8010.0f};  // Near other track items
    
    spdlog::info("Spawned ice wall with ID {} at position ({}, {}, {})", 
      iceWall.itemId, iceWall.position[0], iceWall.position[1], iceWall.position[2]);
    
    // Notify all clients about the ice wall spawn using proper race item spawn command
    protocol::AcCmdGameRaceItemSpawn iceWallSpawn{
      .itemId = iceWall.itemId,
      .itemType = iceWall.itemType,
      .position = iceWall.position,
      .orientation = {0.0f, 0.0f, 0.0f, 1.0f},
      .sizeLevel = false,
      .removeDelay = -1  // Use same as working items (no removal)
    };
    
    spdlog::info("Sending ice wall spawn using AcCmdGameRaceItemSpawn: itemId={}, position=({}, {}, {})", 
      iceWallSpawn.itemId, iceWallSpawn.position[0], iceWallSpawn.position[1], iceWallSpawn.position[2]);
    
    spdlog::info("Broadcasting to {} clients in room", roomInstance.clients.size());
    for (const ClientId& roomClientId : roomInstance.clients)
    {
      spdlog::info("Sending ice wall spawn to client {}", roomClientId);
      _commandServer.QueueCommand<decltype(iceWallSpawn)>(
        roomClientId, 
        [iceWallSpawn]() { return iceWallSpawn; });
    }
  }

  racer.magicItem.reset();
}

void RaceDirector::HandleUserRaceItemGet(
  ClientId clientId,
  const protocol::AcCmdUserRaceItemGet& command)
{
  const auto& clientContext = _clients[clientId];
  auto& roomInstance = _roomInstances[clientContext.roomUid];
  auto const& item = roomInstance.tracker.GetItems().at(command.itemId);
  protocol::AcCmdGameRaceItemGet get{
    .characterOid = command.characterOid,
    .itemId = command.itemId,
    .itemType = item.itemType,
  };

  // Notify all clients in the room that this item has been picked up
  for (const ClientId& roomClientId : roomInstance.clients)
  {
    _commandServer.QueueCommand<decltype(get)>(
      roomClientId,
      [get]()
      {
        return get;
      });
  }
  // Wait for ItemDeck registry, to give the correct amount of SP for item pick up

  _scheduler.Queue(
    [this, clientId, item, &roomInstance]()
    {
      // Respawn the item after a delay
      protocol::AcCmdGameRaceItemSpawn spawn{
        .itemId = item.itemId,
        .itemType = item.itemType,
        .position = item.position,
        .orientation = {0.0f, 0.0f, 0.0f, 1.0f},
        .sizeLevel = false,
        .removeDelay = -1
      };

      for (const ClientId& roomClientId : roomInstance.clients)
      {
        _commandServer.QueueCommand<decltype(spawn)>(
          roomClientId, 
          [spawn]()
          {
            return spawn;
          });
      }
    },
    //only for speed for now, change to itemDeck registry later for magic
    Scheduler::Clock::now() + std::chrono::milliseconds(500));
}

// Magic Targeting System Implementation for Bolt
void RaceDirector::HandleStartMagicTarget(
  ClientId clientId,
  const protocol::AcCmdCRStartMagicTarget& command)
{
  spdlog::info("Player {} started magic targeting with targetOrParam {}", 
    clientId, command.targetOrParam);
  
  const auto& clientContext = _clients[clientId];
  auto& roomInstance = _roomInstances[clientContext.roomUid];
  auto& racer = roomInstance.tracker.GetRacer(clientContext.characterUid);
  
  // Set targeting state
  racer.isTargeting = true;
  racer.currentTarget = tracker::InvalidEntityOid;
  
  spdlog::info("Character {} (OID {}) entered targeting mode", clientContext.characterUid, racer.oid);
}

void RaceDirector::HandleChangeMagicTargetNotify(
  ClientId clientId,
  const protocol::AcCmdCRChangeMagicTargetNotify& command)
{
  spdlog::info("Player {} changed magic target: v0={}, v1={}, v2={}, v3={}", 
    clientId, command.v0, command.v1, command.v2, command.v3);
  
  const auto& clientContext = _clients[clientId];
  auto& roomInstance = _roomInstances[clientContext.roomUid];
  auto& racer = roomInstance.tracker.GetRacer(clientContext.characterUid);
  
  uint16_t characterOid = command.v0;  // Attacker OID
  uint16_t targetOid = command.v1;     // Victim OID
  
  if (characterOid != racer.oid)
  {
    spdlog::warn("Character OID mismatch in HandleChangeMagicTargetNotify");
    return;
  }
  
  // Update current target
  racer.currentTarget = targetOid;
  
  // Broadcast targeting notification to ALL players in room
  // (so everyone can see the targeting indicator)
  protocol::AcCmdCRChangeMagicTargetNotify targetNotify{
    .v0 = command.v0,
    .v1 = command.v1,
    .v2 = command.v2,
    .v3 = command.v3
  };
  
  for (const ClientId& roomClientId : roomInstance.clients)
  {
    _commandServer.QueueCommand<decltype(targetNotify)>(
      roomClientId, 
      [targetNotify]() { return targetNotify; });
  }
}

void RaceDirector::HandleChangeMagicTargetOK(
  ClientId clientId,
  const protocol::AcCmdCRChangeMagicTargetOK& command)
{
  spdlog::info("Player {} confirmed magic target: character OID {} -> target OID {}, f08={}, f0A={}", 
    clientId, command.characterOid, command.targetOid, command.f08, command.f0A);
  
  const auto& clientContext = _clients[clientId];
  auto& roomInstance = _roomInstances[clientContext.roomUid];
  auto& racer = roomInstance.tracker.GetRacer(clientContext.characterUid);
  
  if (command.characterOid != racer.oid)
  {
    spdlog::warn("Character OID mismatch in HandleChangeMagicTargetOK");
    return;
  }
  
  // This is where the Bolt should be fired!
  spdlog::info("BOLT FIRED! {} -> {}", command.characterOid, command.targetOid);
  
  // Find the target racer and apply bolt effects
  for (auto& [targetUid, targetRacer] : roomInstance.tracker.GetRacers())
  {
    if (targetRacer.oid == command.targetOid)
    {
      spdlog::info("Bolt hit target {}! Applying effects...", command.targetOid);
      
      // Apply bolt effects: fall down, lose speed, lose item
      // Reset their magic item (they lose it when hit)
      if (targetRacer.magicItem.has_value())
      {
        uint32_t lostItemId = targetRacer.magicItem.value();
        targetRacer.magicItem.reset();
        
        // Send explicit MagicExpire command to victim
        protocol::AcCmdRCMagicExpire expireNotify{};
        expireNotify.magicId = lostItemId;
        expireNotify.actorId = static_cast<uint16_t>(command.targetOid);
        expireNotify.magicCode = static_cast<uint16_t>(lostItemId);
        expireNotify.reasonFlag = 1;  // Hit by bolt (reason: consumed/stolen)
        
        // Find victim's client and send
        for (const auto& roomClientId : roomInstance.clients)
        {
          const auto& roomClientContext = _clients[roomClientId];
          auto& roomClientRacer = roomInstance.tracker.GetRacer(roomClientContext.characterUid);
          if (roomClientRacer.oid == command.targetOid)
          {
            _commandServer.QueueCommand<decltype(expireNotify)>(
              roomClientId,
              [expireNotify]() { return expireNotify; });
            spdlog::info("Sent MagicExpire to victim client {} (OID {})", roomClientId, command.targetOid);
            break;
          }
        }
      }
      
      // Send bolt hit notification to all clients so they can see the hit effects
      spdlog::info("Sending bolt hit notification to all clients for target {}", command.targetOid);
      
      // Send bolt hit as magic item usage notification
      protocol::AcCmdCRUseMagicItemNotify boltHitNotify{};
      boltHitNotify.actor_oid = static_cast<uint16_t>(command.characterOid);  // Caster/attacker OID                                                                                   
      boltHitNotify.magic_type = 2;  // Bolt magic item ID

      // No spatial data for bolt
      boltHitNotify.spatial = std::nullopt;

      // CRITICAL: Include target OID in IDs array so client can apply knockdown
      boltHitNotify.payload.ids_count = 1;  // One target
      boltHitNotify.payload.ids[0] = static_cast<int16_t>(command.targetOid);  // Target OID     
      for (uint8_t i = 1; i < 8; ++i)
      {
        boltHitNotify.payload.ids[i] = 0;  // Zero out unused slots
      }

      // Tail fields
      boltHitNotify.tail_u16 = static_cast<uint16_t>(command.targetOid);  // Target OID 
      boltHitNotify.tail_u32 = 0;
      
      for (const ClientId& roomClientId : roomInstance.clients)
      {
        spdlog::info("Sending bolt hit notification to client {}", roomClientId);
        _commandServer.QueueCommand<decltype(boltHitNotify)>(
          roomClientId, 
          [boltHitNotify]() { return boltHitNotify; });
      }
      
      break;
    }
  }
  
  // Reset attacker's targeting state
  racer.isTargeting = false;
  racer.currentTarget = tracker::InvalidEntityOid;
  
  // Consume the Bolt magic item
  racer.magicItem.reset();
}

void RaceDirector::HandleChangeMagicTargetCancel(
  ClientId clientId,
  const protocol::AcCmdCRChangeMagicTargetCancel& command)
{
  spdlog::info("Player {} cancelled magic targeting: character OID {}", 
    clientId, command.characterOid);
  
  const auto& clientContext = _clients[clientId];
  auto& roomInstance = _roomInstances[clientContext.roomUid];
  auto& racer = roomInstance.tracker.GetRacer(clientContext.characterUid);
  
  if (command.characterOid != racer.oid)
  {
    spdlog::warn("Character OID mismatch in HandleChangeMagicTargetCancel");
    return;
  }
  
  // Send remove target notification to the current target (if any)
  if (racer.currentTarget != tracker::InvalidEntityOid)
  {
    protocol::AcCmdRCRemoveMagicTarget removeNotify{
      .characterOid = command.characterOid
    };
    
    // Find the client ID for the current target
    for (const ClientId& roomClientId : roomInstance.clients)
    {
      const auto& targetClientContext = _clients[roomClientId];
      if (roomInstance.tracker.GetRacer(targetClientContext.characterUid).oid == racer.currentTarget)
      {
        _commandServer.QueueCommand<decltype(removeNotify)>(
          roomClientId, 
          [removeNotify]() { return removeNotify; });
        break;
      }
    }
  }
  
  // Reset targeting state
  racer.isTargeting = false;
  racer.currentTarget = tracker::InvalidEntityOid;
  
  spdlog::info("Character {} exited targeting mode", command.characterOid);
}

void RaceDirector::HandleTriggerEvent(
  ClientId clientId,
  const protocol::AcCmdCRTriggerEvent& command)
{
  spdlog::info("Player {} triggered event: event_id={}, arg={}", 
    clientId, command.event_id, command.arg);
  
  const auto& clientContext = _clients[clientId];
  auto& roomInstance = _roomInstances[clientContext.roomUid];
  
  // Get the sender's OID from the room tracker
  auto& racer = roomInstance.tracker.GetRacer(clientContext.characterUid);
  
  // This command is for client-initiated trigger events
  
  protocol::AcCmdRCTriggerActivate response{
    .trigger_id = command.event_id,
    .state = static_cast<uint16_t>(command.arg)  // Use arg as state
  };
  
  // Broadcast trigger activation to all clients in the room
  for (const ClientId& roomClientId : roomInstance.clients)
  {
    _commandServer.QueueCommand<decltype(response)>(
      roomClientId,
      [response]() { return response; });
  }
  
  spdlog::debug("Broadcasted trigger activation for event {} to all clients in room", 
    command.event_id);
}

void RaceDirector::HandleActivateSkillEffect(
  ClientId clientId,
  const protocol::AcCmdCRActivateSkillEffect& command)
{
  spdlog::info("Player {} activated skill effect: field_04={}, field_08={}, field_0C={}, field_0E={}, field_10={}", 
    clientId, command.field_04, command.field_08, command.field_0C, command.field_0E, command.field_10);
  
  // This command appears to be sent by the client when visual effects should be triggered
  // For bolt magic: field_04=2 (target OID?), field_08=0, field_0C=1, field_0E=1, field_10=1.0
  // 
  // The server doesn't need to respond to this - it's informational
  // Client is telling server "I'm playing this skill effect animation"
  // 
  // In the future, this could be used for:
  // - Anti-cheat validation (did client play expected effect?)
  // - Server-side effect tracking
  // - Replays/spectators
  
  // For now, just log and acknowledge
  spdlog::debug("Handled ActivateSkillEffect from client {}", clientId);
}


} // namespace server

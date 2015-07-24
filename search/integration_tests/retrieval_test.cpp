#include "testing/testing.hpp"

#include "indexer/classificator_loader.hpp"
#include "indexer/index.hpp"
#include "indexer/mwm_set.hpp"
#include "indexer/scales.hpp"
#include "indexer/search_delimiters.hpp"
#include "indexer/search_string_utils.hpp"

#include "search/integration_tests/test_mwm_builder.hpp"
#include "search/retrieval.hpp"
#include "search/search_query_params.hpp"

#include "platform/local_country_file.hpp"
#include "platform/platform.hpp"

#include "base/scope_guard.hpp"
#include "base/string_utils.hpp"

namespace
{
void InitParams(string const & query, search::SearchQueryParams & params)
{
  params.Clear();
  auto insertTokens = [&params](strings::UniString const & token)
  {
    params.m_tokens.push_back({token});
  };
  search::Delimiters delims;
  SplitUniString(search::NormalizeAndSimplifyString(query), insertTokens, delims);

  params.m_langs.insert(StringUtf8Multilang::GetLangIndex("en"));
}

class TestCallback : public search::Retrieval::Callback
{
public:
  TestCallback(MwmSet::MwmId const & id) : m_id(id), m_triggered(false) {}

  // search::Retrieval::Callback overrides:
  void OnMwmProcessed(MwmSet::MwmId const & id, vector<uint32_t> const & offsets) override
  {
    TEST(!m_triggered, ("Callback must be triggered only once."));
    TEST_EQUAL(m_id, id, ());
    m_triggered = true;
    m_offsets = offsets;
  }

  bool WasTriggered() const { return m_triggered; }

  vector<uint32_t> & Offsets() { return m_offsets; }
  vector<uint32_t> const & Offsets() const { return m_offsets; }

private:
  MwmSet::MwmId const m_id;
  vector<uint32_t> m_offsets;
  bool m_triggered;
};

class MultiMwmCallback : public search::Retrieval::Callback
{
public:
  MultiMwmCallback(vector<MwmSet::MwmId> const & ids) : m_ids(ids), m_numFeatures(0) {}

  // search::Retrieval::Callback overrides:
  void OnMwmProcessed(MwmSet::MwmId const & id, vector<uint32_t> const & offsets) override
  {
    auto const it = find(m_ids.cbegin(), m_ids.cend(), id);
    TEST(it != m_ids.cend(), ("Unknown mwm:", id));

    auto const rt = m_retrieved.find(id);
    TEST(rt == m_retrieved.cend(), ("For", id, "callback must be triggered only once."));

    m_retrieved.insert(id);
    m_numFeatures += offsets.size();
  }

  uint64_t GetNumMwms() const { return m_retrieved.size(); }

  uint64_t GetNumFeatures() const { return m_numFeatures; }

private:
  vector<MwmSet::MwmId> m_ids;
  set<MwmSet::MwmId> m_retrieved;
  uint64_t m_numFeatures;
};
}  // namespace

UNIT_TEST(Retrieval_Smoke)
{
  classificator::Load();
  Platform & platform = GetPlatform();

  platform::LocalCountryFile file(platform.WritableDir(), platform::CountryFile("WhiskeyTown"), 0);
  MY_SCOPE_GUARD(deleteFile, [&]()
  {
    file.DeleteFromDisk(MapOptions::Map);
  });

  // Create a test mwm with 100 whiskey bars.
  {
    TestMwmBuilder builder(file);
    for (int x = 0; x < 10; ++x)
    {
      for (int y = 0; y < 10; ++y)
        builder.AddPOI(m2::PointD(x, y), "Whiskey bar", "en");
    }
  }
  TEST_EQUAL(MapOptions::Map, file.GetFiles(), ());

  Index index;
  auto p = index.RegisterMap(file);
  auto & handle = p.first;
  TEST(handle.IsAlive(), ());
  TEST_EQUAL(p.second, MwmSet::RegResult::Success, ());

  search::SearchQueryParams params;
  InitParams("whiskey bar", params);

  search::Retrieval retrieval;

  // Retrieve all (100) whiskey bars from the mwm.
  {
    TestCallback callback(handle.GetId());

    retrieval.Init(index, m2::RectD(m2::PointD(0, 0), m2::PointD(1, 1)), params,
                   search::Retrieval::Limits());
    retrieval.Go(callback);
    TEST(callback.WasTriggered(), ());
    TEST_EQUAL(100, callback.Offsets().size(), ());

    TestCallback dummyCallback(handle.GetId());
    retrieval.Go(dummyCallback);
    TEST(!dummyCallback.WasTriggered(), ());
  }

  // Retrieve all whiskey bars from the left-bottom 5 x 5 square.
  {
    TestCallback callback(handle.GetId());
    search::Retrieval::Limits limits;
    limits.SetMaxViewportScale(5.0);

    retrieval.Init(index, m2::RectD(m2::PointD(0, 0), m2::PointD(1, 1)), params, limits);
    retrieval.Go(callback);
    TEST(callback.WasTriggered(), ());
    TEST_EQUAL(36 /* number of whiskey bars in a 5 x 5 square (border is counted) */,
               callback.Offsets().size(), ());
  }

  // Retrieve at least than 8 whiskey bars from the center.
  {
    TestCallback callback(handle.GetId());
    search::Retrieval::Limits limits;
    limits.SetMinNumFeatures(8);

    retrieval.Init(index, m2::RectD(m2::PointD(4.9, 4.9), m2::PointD(5.1, 5.1)), params, limits);
    retrieval.Go(callback);
    TEST(callback.WasTriggered(), ());
    TEST_GREATER_OR_EQUAL(callback.Offsets().size(), 8, ());
  }
}

UNIT_TEST(Retrieval_3Mwms)
{
  classificator::Load();
  Platform & platform = GetPlatform();

  platform::LocalCountryFile msk(platform.WritableDir(), platform::CountryFile("msk"), 0);
  platform::LocalCountryFile mtv(platform.WritableDir(), platform::CountryFile("mtv"), 0);
  platform::LocalCountryFile zrh(platform.WritableDir(), platform::CountryFile("zrh"), 0);
  MY_SCOPE_GUARD(deleteFiles, [&]()
  {
    msk.DeleteFromDisk(MapOptions::Map);
    mtv.DeleteFromDisk(MapOptions::Map);
    zrh.DeleteFromDisk(MapOptions::Map);
  });

  {
    TestMwmBuilder builder(msk);
    builder.AddPOI(m2::PointD(0, 0), "Cafe MTV", "en");
  }
  {
    TestMwmBuilder builder(mtv);
    builder.AddPOI(m2::PointD(10, 0), "MTV", "en");
  }
  {
    TestMwmBuilder builder(zrh);
    builder.AddPOI(m2::PointD(0, 10), "Bar MTV", "en");
  }

  Index index;
  auto mskP = index.RegisterMap(msk);
  auto & mskHandle = mskP.first;

  auto mtvP = index.RegisterMap(mtv);
  auto & mtvHandle = mtvP.first;

  auto zrhP = index.RegisterMap(zrh);
  auto & zrhHandle = zrhP.first;

  TEST(mskHandle.IsAlive(), ());
  TEST(mtvHandle.IsAlive(), ());
  TEST(zrhHandle.IsAlive(), ());

  search::SearchQueryParams params;
  InitParams("mtv", params);

  search::Retrieval retrieval;

  {
    TestCallback callback(mskHandle.GetId());
    search::Retrieval::Limits limits;
    limits.SetMinNumFeatures(1);

    retrieval.Init(index, m2::RectD(m2::PointD(-1.0, -1.0), m2::PointD(1.0, 1.0)), params, limits);
    retrieval.Go(callback);
    TEST(callback.WasTriggered(), ());
    TEST_EQUAL(callback.Offsets().size(), 1, ());
  }

  {
    MultiMwmCallback callback({mskHandle.GetId(), mtvHandle.GetId(), zrhHandle.GetId()});
    search::Retrieval::Limits limits;

    retrieval.Init(index, m2::RectD(m2::PointD(-1.0, -1.0), m2::PointD(1.0, 1.0)), params, limits);
    retrieval.Go(callback);
    TEST_EQUAL(3, callback.GetNumMwms(), ());
    TEST_EQUAL(3, callback.GetNumFeatures(), ());
  }
}

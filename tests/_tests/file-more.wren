var created = File.create("t-file.txt", "text/plain", "t-content")
var id = created.id
Test.assert(id != null, "File.create returns id")
Test.assert(created.name == "t-file.txt", "File.create name")
Test.assert(created.type == "text/plain", "File.create type")
Test.assert(created.size == "t-content".count, "File.create size")
Test.assert(!created.isTemp, "File created permanent")

var fetched = File.get(id)
Test.assert(fetched.id == id, "File.get returns file")
Test.assert(fetched.createdAt != null, "File.get populates createdAt")

created.temp
Test.assert(created.isTemp, "File.temp marks temporary")
Test.assert(File.get(id).id == null, "File.get skips temporary files")

created.save
Test.assert(File.get(id).id == id, "File.save restores permanence")

created.destroy
Test.assert(File.get(id).id == null, "File.destroy deletes file")
